
//
// THIS IS A DEMO SOURCE CODE FOR GMP LIBRARY.
//
// User should define your own controller objects,
// and initilize them.
//
// User should implement a ctl loop function, this
// function would be called every main loop.
//
// User should implement a state machine if you are using
// Controller Nanon framework.
//

#include <gmp_core.h>

#include "ctl_settings_defaults.h"

#include "ctl_main.h"

#include <xplt.peripheral.h>

#include <core/dev/pil_core.h>

//=================================================================================================
// global controller variables

// System framework
cia402_sm_t cia402_sm;

// Control Law Core
// Current controller, Power controller / Voltage controller
gfl_pq_ctrl_t pq_ctrl;
inv_neg_ctrl_init_t gfl_neg_init;
inv_neg_ctrl_t neg_current_ctrl;
gfl_inv_ctrl_init_t gfl_init;
gfl_inv_ctrl_t inv_ctrl;
#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7
gfl_level67_voltage_ctrl_t voltage_ctrl;
#endif

// Input channel

// Output channel: SPWM modulator / SVPWM modulator / NPC modulator
#if defined USING_NPC_MODULATOR
npc_modulator_t spwm;
#else
spwm_modulator_t spwm;
#endif // USING_NPC_MODULATOR

// Protection module

// ADC Calibrator
adc_bias_calibrator_t adc_calibrator;
#if defined SPECIFY_ENABLE_ADC_CALIBRATE
volatile fast_gt flag_enable_adc_calibrator = 1;
#else
volatile fast_gt flag_enable_adc_calibrator = 0;
#endif
volatile fast_gt index_adc_calibrator = 0;
uint32_t pq_loop_tick = 0;

// User commands
volatile fast_gt ctl_user_run_request = 0;
volatile uint16_t ctl_output_frequency_hz = (uint16_t)(GFL_GRID_FREQUENCY_HZ + 0.5f);
volatile uint16_t ctl_output_frequency_request_hz = (uint16_t)(GFL_GRID_FREQUENCY_HZ + 0.5f);
volatile uint16_t ctl_dc_bus_voltage_setting_v = GFL_UI_DCBUS_LOW_V;
volatile uint16_t ctl_dc_bus_voltage_request_v = GFL_UI_DCBUS_LOW_V;
volatile uint16_t ctl_voltage_ref_profile = 0U;
volatile uint16_t ctl_voltage_ref_profile_request = 0U;
volatile float ctl_voltage_ref_pu = GFL_LEVEL6_VD_REF_PU;
volatile ctrl_gt ctl_dc_bus_feedforward_gain = float2ctrl(1.0f);

static float ctl_get_voltage_ref_pu(uint16_t profile)
{
    switch (profile)
    {
    case 1U:
        return GFL_LEVEL6_VD_REF_LOW_MID_PU;
    case 2U:
        return GFL_LEVEL6_VD_REF_MID_PU;
    case 3U:
        return GFL_LEVEL6_VD_REF_HIGH_PU;
    case 4U:
        return GFL_LEVEL6_VD_REF_MAX_PU;
    default:
        return GFL_LEVEL6_VD_REF_PU;
    }
}

static void ctl_update_level67_voltage_reference(void)
{
#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7
    ctl_voltage_ref_pu = ctl_get_voltage_ref_pu(ctl_voltage_ref_profile);
    voltage_ctrl.vdq_set.dat[phase_d] =
        float2ctrl((parameter_gt)ctl_voltage_ref_pu *
                   (parameter_gt)ctl_dc_bus_voltage_setting_v / (parameter_gt)CTRL_DCBUS_VOLTAGE);
    voltage_ctrl.vdq_set.dat[phase_q] = float2ctrl(GFL_LEVEL6_VQ_REF_PU);
#else
    ctl_voltage_ref_pu = ctl_get_voltage_ref_pu(ctl_voltage_ref_profile);
#endif
}

//=================================================================================================
// CTL initialize routine

void ctl_init()
{
    //
    // stop here and wait for user start the motor controller
    //
    ctl_fast_disable_output();
    ctl_user_run_request = 0;
    ctl_output_frequency_hz = (uint16_t)(GFL_GRID_FREQUENCY_HZ + 0.5f);
    ctl_output_frequency_request_hz = ctl_output_frequency_hz;
    ctl_dc_bus_voltage_setting_v = GFL_UI_DCBUS_LOW_V;
    ctl_dc_bus_voltage_request_v = ctl_dc_bus_voltage_setting_v;
    ctl_voltage_ref_profile = 0U;
    ctl_voltage_ref_profile_request = 0U;
    ctl_voltage_ref_pu = GFL_LEVEL6_VD_REF_PU;
    ctl_dc_bus_feedforward_gain = float2ctrl(1.0f);

    //
    // GFL inverter init objects
    //
    gfl_init.fs = CONTROLLER_FREQUENCY;
    gfl_init.v_base = CTRL_VOLTAGE_BASE;
    gfl_init.v_grid = GFL_GRID_VOLTAGE_PU;
    gfl_init.i_base = CTRL_CURRENT_BASE;
    gfl_init.freq_base = GFL_GRID_FREQUENCY_HZ;

    gfl_init.grid_filter_L = GFL_GRID_FILTER_INDUCTANCE_H;
    gfl_init.grid_filter_C = GFL_GRID_FILTER_CAPACITANCE_F;

    ctl_auto_tuning_gfl_inv(&gfl_init);
#if BUILD_LEVEL == 2
    // Level 2 is the first hardware current-loop test. Use a lower input
    // bandwidth to reject PWM-related ADC noise without materially reducing
    // phase margin at the auto-tuned current-loop crossover.
    gfl_init.current_adc_fc = GFL_LEVEL2_CURRENT_ADC_FILTER_FC_HZ;
#endif
    ctl_init_gfl_inv(&inv_ctrl, &gfl_init);

#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7
    // Use a dedicated, lower-bandwidth DC-bus filter for both the OLED
    // measurement and feedforward. This rejects PWM/ADC noise without
    // slowing the phase-voltage and phase-current feedback channels.
    ctl_init_filter_iir1_lpf(&inv_ctrl.filter_udc, CONTROLLER_FREQUENCY,
                             GFL_LEVEL67_DCBUS_FILTER_FC_HZ);
#endif

    ctl_auto_tuning_neg_inv(&gfl_neg_init, &gfl_init);

#if BUILD_LEVEL == 6
    gfl_neg_init.seq_filter_q = GFL_LEVEL6_NEG_NOTCH_Q;
    gfl_neg_init.kp_current = GFL_LEVEL6_NEG_CURRENT_KP;
    gfl_neg_init.ki_current = GFL_LEVEL6_NEG_CURRENT_KI;
    gfl_neg_init.limit_current_out = GFL_LEVEL6_NEG_CURRENT_LIMIT_PU;
    gfl_neg_init.kp_voltage = GFL_LEVEL6_NEG_VOLTAGE_KP;
    gfl_neg_init.ki_voltage = GFL_LEVEL6_NEG_VOLTAGE_KI;
    gfl_neg_init.limit_voltage_out = GFL_LEVEL6_NEG_VOLTAGE_LIMIT_PU;
#elif BUILD_LEVEL == 7
    gfl_neg_init.seq_filter_q = GFL_LEVEL6_NEG_NOTCH_Q;
    gfl_neg_init.kp_current = GFL_LEVEL7_NEG_CURRENT_KP;
    gfl_neg_init.ki_current = GFL_LEVEL7_NEG_CURRENT_KI;
    gfl_neg_init.limit_current_out = GFL_LEVEL7_NEG_CURRENT_LIMIT_PU;
    gfl_neg_init.kp_voltage = GFL_LEVEL7_NEG_VOLTAGE_KP;
    gfl_neg_init.ki_voltage = GFL_LEVEL7_NEG_VOLTAGE_KI;
    gfl_neg_init.limit_voltage_out = GFL_LEVEL7_NEG_VOLTAGE_LIMIT_PU;
#endif

    ctl_init_neg_inv(&neg_current_ctrl, &gfl_neg_init);
    ctl_attach_neg_inv_to_gfl(&neg_current_ctrl, &inv_ctrl);

#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7
    ctl_init_level67_voltage(&voltage_ctrl, CONTROLLER_FREQUENCY, GFL_GRID_FREQUENCY_HZ,
                             GFL_LEVEL6_VOLTAGE_D_KP, GFL_LEVEL6_VOLTAGE_D_KI,
                             GFL_LEVEL6_VOLTAGE_Q_KP, GFL_LEVEL6_VOLTAGE_Q_KI,
                             GFL_LEVEL6_CURRENT_LIMIT_PU, GFL_LEVEL6_POS_VOLTAGE_NOTCH_Q,
                             GFL_LEVEL7_DDSRF_FILTER_FC_HZ, GFL_LEVEL67_SOFT_START_TIME_MS);
    // CTRL_DCBUS_VOLTAGE remains the fixed ADC/per-unit base. The selected
    // operating-bus voltage derates the physical AC-voltage target without
    // changing sensor calibration at run time.
    ctl_update_level67_voltage_reference();
#endif

    //
    // init SPWM modulator
    //
#if defined USING_NPC_MODULATOR
    ctl_init_npc_modulator(&spwm, CTRL_PWM_CMP_MAX, CTRL_PWM_DEADBAND_CMP, &inv_ctrl.adc_iabc->value, float2ctrl(0.02),
                           float2ctrl(0.005));
#else
    ctl_init_spwm_modulator(&spwm, CTRL_PWM_CMP_MAX, CTRL_PWM_DEADBAND_CMP, &inv_ctrl.adc_iabc->value, float2ctrl(0.02),
                            float2ctrl(0.005));
#endif // USING_NPC_MODULATOR

    //
    // Power controller
    //
    ctl_init_gfl_pq(&pq_ctrl, GFL_PQ_ACTIVE_KP, GFL_PQ_ACTIVE_KI, GFL_PQ_REACTIVE_KP, GFL_PQ_REACTIVE_KI,
                    GFL_PQ_CURRENT_LIMIT_PU, GFL_PQ_LOOP_FREQUENCY_HZ);
    ctl_attach_gfl_pq_to_core(&pq_ctrl, &inv_ctrl);
    ctl_set_gfl_pq_ref(&pq_ctrl, float2ctrl(GFL_ACTIVE_POWER_REF_PU), float2ctrl(GFL_REACTIVE_POWER_REF_PU));
    pq_loop_tick = 0;

#if BUILD_LEVEL == 1
    // Voltage open loop, inverter
    ctl_set_gfl_inv_openloop_mode(&inv_ctrl);
    ctl_set_gfl_inv_voltage_openloop(&inv_ctrl, float2ctrl(GFL_OPEN_LOOP_VD_PU), float2ctrl(GFL_OPEN_LOOP_VQ_PU));

#elif BUILD_LEVEL == 2
    // Basic current close loop, inverter
    ctl_set_gfl_inv_current_mode(&inv_ctrl);
    ctl_set_gfl_inv_current(&inv_ctrl, float2ctrl(GFL_CURRENT_LEVEL2_ID_PU), float2ctrl(GFL_CURRENT_LEVEL2_IQ_PU));

#elif BUILD_LEVEL == 3
    // Basic current close loop, inverter
    ctl_set_gfl_inv_current_mode(&inv_ctrl);
    ctl_set_gfl_inv_current(&inv_ctrl, float2ctrl(GFL_CURRENT_LEVEL3_ID_PU), float2ctrl(GFL_CURRENT_LEVEL3_IQ_PU));

    ctl_enable_neg_current_inv(&neg_current_ctrl);

    ctl_enable_gfl_inv_pll(&inv_ctrl);
    ctl_set_gfl_inv_grid_connect(&inv_ctrl);

#elif BUILD_LEVEL == 4
    // current close loop with feed forward, inverter
    ctl_set_gfl_inv_current_mode(&inv_ctrl);
    ctl_set_gfl_inv_current(&inv_ctrl, float2ctrl(GFL_CURRENT_LEVEL4_ID_PU), float2ctrl(GFL_CURRENT_LEVEL4_IQ_PU));

    ctl_enable_neg_current_inv(&neg_current_ctrl);

    ctl_enable_gfl_inv_pll(&inv_ctrl);
    ctl_set_gfl_inv_grid_connect(&inv_ctrl);

    ctl_enable_gfl_inv_decouple(&inv_ctrl);
    ctl_enable_gfl_inv_active_damp(&inv_ctrl);
    ctl_enable_gfl_inv_lead_compensator(&inv_ctrl);

#elif BUILD_LEVEL == 5
    // Cascaded P/Q power loop -> d/q current loop, grid connected.
    ctl_set_gfl_inv_current_mode(&inv_ctrl);
    ctl_set_gfl_inv_current(&inv_ctrl, 0, 0);

    ctl_enable_neg_current_inv(&neg_current_ctrl);
    ctl_enable_gfl_inv_pll(&inv_ctrl);
    ctl_set_gfl_inv_grid_connect(&inv_ctrl);
    ctl_enable_gfl_inv_decouple(&inv_ctrl);
    ctl_enable_gfl_inv_active_damp(&inv_ctrl);
    ctl_enable_gfl_inv_lead_compensator(&inv_ctrl);
    ctl_enable_gfl_pq_ctrl(&pq_ctrl);

#elif BUILD_LEVEL == 6
    // Cascaded positive voltage/current loops with the existing notch-based negative sequence loops.
    ctl_set_gfl_inv_current_mode(&inv_ctrl);
    ctl_set_gfl_inv_current(&inv_ctrl, 0, 0);
    inv_ctrl.flag_enable_current_ctrl = 0;
    inv_ctrl.flag_enable_decouple = 1;
#if GFL_LEVEL67_ENABLE_NEG_SEQ_CTRL != 0
    ctl_enable_neg_voltage_inv(&neg_current_ctrl);
#endif

#elif BUILD_LEVEL == 7
    // Level 6 voltage control with DDSRF positive/negative sequence separation.
    ctl_set_gfl_inv_current_mode(&inv_ctrl);
    ctl_set_gfl_inv_current(&inv_ctrl, 0, 0);
    inv_ctrl.flag_enable_current_ctrl = 0;
    inv_ctrl.flag_enable_decouple = 1;
#if GFL_LEVEL67_ENABLE_NEG_SEQ_CTRL != 0
    ctl_enable_neg_voltage_inv(&neg_current_ctrl);
#endif

#endif // BUILD_LEVEL

    //
    // init and config CiA402 standard state machine
    //
    init_cia402_state_machine(&cia402_sm);
    cia402_sm.minimum_transit_delay[3] = GFL_CIA402_OPERATION_ENABLE_DELAY_MS;
    // The local expansion-board keyboard is the command source. Do not let an
    // unused fieldbus control word override RUN/STOP key commands.
    cia402_sm.flag_enable_control_word = 0;

#if defined SPECIFY_PC_ENVIRONMENT
    cia402_sm.flag_enable_control_word = 0;
    cia402_sm.current_cmd = CIA402_CMD_ENABLE_OPERATION;
#endif // SPECIFY_PC_ENVIRONMENT

#if BUILD_LEVEL >= 3

    // NOTICE:
    // if grid connect is request disable switch delay from CIA402_SM_SWITCH_ON_DISABLED to CIA402_SM_SWITCHED_ON
    // or a longer judgment time can lead to failure to connect to the grid.
    cia402_sm.minimum_transit_delay[CIA402_SM_READY_TO_SWITCH_ON] = 0;
    cia402_sm.minimum_transit_delay[CIA402_SM_SWITCHED_ON] = 0;

#endif // BUILD_LEVEL

    //
    // init ADC Calibrator
    //
    ctl_init_adc_calibrator(&adc_calibrator, GFL_ADC_CALIBRATOR_FC_HZ, GFL_ADC_CALIBRATOR_Q, CONTROLLER_FREQUENCY);

    if (flag_enable_adc_calibrator)
    {
        ctl_enable_adc_calibrator(&adc_calibrator);
    }
}

//=================================================================================================
// CTL endless loop routine

void ctl_set_run_request(fast_gt enable)
{
    ctl_user_run_request = enable ? 1 : 0;

    if (ctl_user_run_request)
        cia402_send_cmd(&cia402_sm, CIA402_CMD_ENABLE_OPERATION);
    else
        cia402_send_cmd(&cia402_sm, CIA402_CMD_DISABLE_VOLTAGE);
}

void ctl_request_output_frequency_hz(uint16_t frequency_hz)
{
    if ((frequency_hz != 30U) && (frequency_hz != 60U))
        return;

    ctl_output_frequency_request_hz = frequency_hz;

    // Frequency-dependent filters are reconfigured only after PWM has
    // stopped. The operator must press RUN again after changing frequency.
    if (inv_ctrl.flag_enable_system)
        ctl_set_run_request(0);
}

void ctl_apply_output_frequency_request(void)
{
    uint16_t frequency_hz = ctl_output_frequency_request_hz;
    ctrl_gt preserved_angle;
    int i;

    if ((frequency_hz == ctl_output_frequency_hz) ||
        ((frequency_hz != 30U) && (frequency_hz != 60U)))
        return;

    // Do not change frequency-dependent controller coefficients while the
    // PWM/control ISR is active.
    if (inv_ctrl.flag_enable_system)
        return;

    preserved_angle = inv_ctrl.rg.current;
    ctl_set_ramp_generator_slope(
        &inv_ctrl.rg, float2ctrl((parameter_gt)frequency_hz / (parameter_gt)CONTROLLER_FREQUENCY));
    inv_ctrl.rg.current = preserved_angle;

    gfl_init.freq_base = (parameter_gt)frequency_hz;
    inv_ctrl.coef_ff_decouple =
        float2ctrl(CTL_PARAM_CONST_2PI * gfl_init.grid_filter_L * (parameter_gt)frequency_hz *
                   gfl_init.i_base / gfl_init.v_base);

    // Level 6 uses these 2-omega notches directly. Level 7 bypasses them, but
    // keeping the coefficients synchronized makes later build-level changes
    // and CCS inspection unambiguous.
    gfl_neg_init.freq_base = (parameter_gt)frequency_hz;
    for (i = 0; i < 2; ++i)
    {
        ctl_init_biquad_notch(&neg_current_ctrl.filter_idqn[i], gfl_neg_init.fs,
                              2.0f * (parameter_gt)frequency_hz, gfl_neg_init.seq_filter_q);
        ctl_init_biquad_notch(&neg_current_ctrl.filter_vdqn[i], gfl_neg_init.fs,
                              2.0f * (parameter_gt)frequency_hz, gfl_neg_init.seq_filter_q);
    }

#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7
    ctl_init_biquad_notch(&voltage_ctrl.notch_vdq[phase_d], CONTROLLER_FREQUENCY,
                          2.0f * (parameter_gt)frequency_hz, GFL_LEVEL6_POS_VOLTAGE_NOTCH_Q);
    ctl_init_biquad_notch(&voltage_ctrl.notch_vdq[phase_q], CONTROLLER_FREQUENCY,
                          2.0f * (parameter_gt)frequency_hz, GFL_LEVEL6_POS_VOLTAGE_NOTCH_Q);

    // Keep the DDSRF cutoff proportional to the selected fundamental.
    ctl_init_ddsrf_channel(&voltage_ctrl.voltage_seq, CONTROLLER_FREQUENCY,
                           (parameter_gt)frequency_hz * 0.70710678f);
    ctl_init_ddsrf_channel(&voltage_ctrl.current_seq, CONTROLLER_FREQUENCY,
                           (parameter_gt)frequency_hz * 0.70710678f);
#endif

    ctl_output_frequency_hz = frequency_hz;
}

void ctl_request_dc_bus_voltage_v(uint16_t dc_bus_voltage_v)
{
    if ((dc_bus_voltage_v != GFL_UI_DCBUS_LOW_V) &&
        (dc_bus_voltage_v != GFL_UI_DCBUS_HIGH_V))
        return;

    ctl_dc_bus_voltage_request_v = dc_bus_voltage_v;

    // Applying a new voltage target while PWM is active would create an
    // immediate reference step. Stop first and require a deliberate restart.
    if (ctl_user_run_request || inv_ctrl.flag_enable_system)
        ctl_set_run_request(0);
}

void ctl_apply_dc_bus_voltage_request(void)
{
    uint16_t dc_bus_voltage_v = ctl_dc_bus_voltage_request_v;

    if ((dc_bus_voltage_v == ctl_dc_bus_voltage_setting_v) ||
        ((dc_bus_voltage_v != GFL_UI_DCBUS_LOW_V) &&
         (dc_bus_voltage_v != GFL_UI_DCBUS_HIGH_V)))
        return;

    if (inv_ctrl.flag_enable_system)
        return;

    ctl_dc_bus_voltage_setting_v = dc_bus_voltage_v;
    ctl_update_level67_voltage_reference();
}

void ctl_request_voltage_ref_profile(uint16_t profile)
{
    if (profile > 4U)
        return;

    ctl_voltage_ref_profile_request = profile;

    // Apply the new reference only with PWM stopped so the next start still
    // follows the normal voltage-loop soft-start trajectory.
    if (ctl_user_run_request || inv_ctrl.flag_enable_system)
        ctl_set_run_request(0);
}

void ctl_apply_voltage_ref_profile_request(void)
{
    uint16_t profile = ctl_voltage_ref_profile_request;

    if ((profile == ctl_voltage_ref_profile) || (profile > 4U))
        return;

    if (inv_ctrl.flag_enable_system)
        return;

    ctl_voltage_ref_profile = profile;
    ctl_update_level67_voltage_reference();
}

void ctl_mainloop(void)
{
    ctl_apply_output_frequency_request();
    ctl_apply_dc_bus_voltage_request();
    ctl_apply_voltage_ref_profile_request();
    cia402_dispatch(&cia402_sm);

    // A controller/state-machine fault must also clear the operator's switch
    // request. This keeps the OLED state truthful and prevents an automatic
    // restart after the fault is reset.
    if (ctl_user_run_request &&
        ((cia402_sm.current_state == CIA402_SM_FAULT_REACTION) ||
         (cia402_sm.current_state == CIA402_SM_FAULT) ||
         (cia402_sm.last_cb_result <= CIA402_EC_ERROR)))
        ctl_set_run_request(0);

    return;
}

void gmp_pil_sim_step(const gmp_sim_rx_buf_t* rx, gmp_sim_tx_buf_t* tx)
{
#if defined ENABLE_GMP_DL_PIL_SIM
    ctl_input_callback_pil(rx);

    ctl_dispatch();

    ctl_output_callback_pil(tx);
#endif // defined ENABLE_GMP_DL_PIL_SIM
}

#if defined ENABLE_GMP_DL_PIL_SIM
time_gt gmp_base_get_ctrl_tick(void)
{
    return inv_ctrl.isr_tick / ((uint32_t)CONTROLLER_FREQUENCY / 1000);
}
#endif // defined ENABLE_GMP_DL_PIL_SIM

//=================================================================================================
// CiA402 default callback routine

void ctl_enable_pwm()
{
    ctl_fast_enable_output();
}

void ctl_disable_pwm()
{
    ctl_fast_disable_output();
    ctl_disable_gfl_inv(&inv_ctrl);
#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7
    ctl_set_gfl_inv_current(&inv_ctrl, 0, 0);
#endif

    // clear controller here
    ctl_clear_gfl_inv(&inv_ctrl);
    ctl_clear_neg_inv(&neg_current_ctrl);
    ctl_clear_gfl_pq(&pq_ctrl);
#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7
    ctl_clear_level67_voltage(&voltage_ctrl);
#endif
    pq_loop_tick = 0;
}

fast_gt ctl_check_pll_locked(void)
{
    ctrl_gt pll_erro = ctl_abs(ctl_get_gfl_pll_error(&inv_ctrl));

    // grid connected, judge if PLL is ready.
    if (ctl_is_gfl_grid_connected(&inv_ctrl))
    {
        if (pll_erro < CTRL_SPLL_EPSILON)
            return 1;
        else
            return 0;
    }

    // not connect to gird
    else
        return 1;
}

fast_gt ctl_exec_adc_calibration(void)
{
    //
    // 1. ADC Auto calibrate
    //
    if (flag_enable_adc_calibrator)
    {
        if (ctl_is_adc_calibrator_cmpt(&adc_calibrator) && ctl_is_adc_calibrator_result_valid(&adc_calibrator))
        {

            // index_adc_calibrator == 13, for Ibus
            if (index_adc_calibrator == 13)
            {
                // vbus get result
                idc.bias = idc.bias + ctl_div(ctl_get_adc_calibrator_result(&adc_calibrator), idc.gain);

                // move to next position
                index_adc_calibrator += 1;

                // adc calibrate process done.
                flag_enable_adc_calibrator = 0;

                // clear INV controller
                ctl_clear_gfl_inv_with_PLL(&inv_ctrl);

                // ADC Calibrator complete here.
                //ctl_enable_gfl_inv(&inv_ctrl);
            }

            // index_adc_calibrator == 12, for Vbus
            else if (index_adc_calibrator == 12)
            {
                // vbus get result
                //udc.bias = udc.bias + ctl_div(ctl_get_adc_calibrator_result(&adc_calibrator), udc.gain);

                // move to next position
                index_adc_calibrator += 1;

                // clear calibrator
                ctl_clear_adc_calibrator(&adc_calibrator);

                // enable calibrator to next position
                ctl_enable_adc_calibrator(&adc_calibrator);
            }

            // index_adc_calibrator == 11 ~ 9, for Vuvw
            else if (index_adc_calibrator <= 11 && index_adc_calibrator >= 9)
            {
                // vuvw get result
                uuvw.bias[index_adc_calibrator - 9] =
                    uuvw.bias[index_adc_calibrator - 9] +
                    ctl_div(ctl_get_adc_calibrator_result(&adc_calibrator), uuvw.gain[index_adc_calibrator - 9]);

                // move to next position
                index_adc_calibrator += 1;

                // clear calibrator
                ctl_clear_adc_calibrator(&adc_calibrator);

                // enable calibrator to next position
                ctl_enable_adc_calibrator(&adc_calibrator);
            }

            // index_adc_calibrator == 8 ~ 6, for Vabc
            else if (index_adc_calibrator <= 8 && index_adc_calibrator >= 6)
            {
                // vabc get result
                vabc.bias[index_adc_calibrator - 6] =
                    vabc.bias[index_adc_calibrator - 6] +
                    ctl_div(ctl_get_adc_calibrator_result(&adc_calibrator), vabc.gain[index_adc_calibrator - 6]);

                // move to next position
                index_adc_calibrator += 1;

                // clear calibrator
                ctl_clear_adc_calibrator(&adc_calibrator);

                // enable calibrator to next position
                ctl_enable_adc_calibrator(&adc_calibrator);
            }

            // index_adc_calibrator == 5 ~ 3, for Iabc
            else if (index_adc_calibrator <= 5 && index_adc_calibrator >= 3)
            {

                // iabc get result
                iabc.bias[index_adc_calibrator - 3] =
                    iabc.bias[index_adc_calibrator - 3] +
                    ctl_div(ctl_get_adc_calibrator_result(&adc_calibrator), iabc.gain[index_adc_calibrator - 3]);

                // move to next position
                index_adc_calibrator += 1;

                // clear calibrator
                ctl_clear_adc_calibrator(&adc_calibrator);

                // enable calibrator to next position
                ctl_enable_adc_calibrator(&adc_calibrator);
            }

            // index_adc_calibrator == 2 ~ 0, for Iuvw
            else if (index_adc_calibrator <= 2)
            {
                // iuvw get result
                iuvw.bias[index_adc_calibrator] =
                    iuvw.bias[index_adc_calibrator] +
                    ctl_div(ctl_get_adc_calibrator_result(&adc_calibrator), iuvw.gain[index_adc_calibrator]);

                // move to next position
                index_adc_calibrator += 1;

                // clear calibrator
                ctl_clear_adc_calibrator(&adc_calibrator);

                // enable calibrator to next position
                ctl_enable_adc_calibrator(&adc_calibrator);
            }

            // over-range protection
            if (index_adc_calibrator > 13)
                flag_enable_adc_calibrator = 0;
        }

        // ADC calibrate is not complete
        return 0;
    }

    // skip calibrate routine
    return 1;
}
