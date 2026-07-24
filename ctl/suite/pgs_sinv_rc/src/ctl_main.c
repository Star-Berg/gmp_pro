/**
 * @file ctl_main.c
 * @author GMP Library Contributors
 * @brief 20kW Single-Phase Inverter/AFE Top-level Implementation.
 */

#include <gmp_core.h>

#include "ctl_main.h"

//=================================================================================================
// global controller variables

// System framework
cia402_sm_t cia402_sm;

// Control Law Core
spll_sogi_t pll;
ctl_sms_pq_t pq_meter;
ctl_sinv_ref_gen_t ref_gen;
ctl_sinv_rc_core_t rc_core;
ctl_sinv_outer_loop_t outer_loop;
ctl_ramp_generator_t rg;

// FDRC controller static memory
#define FDRC_ARRAY_SIZE ((int)(CONTROLLER_FREQUENCY / CTRL_FDRC_MIN_FREQ) + 10)
static ctrl_gt fdrc_buffer[FDRC_ARRAY_SIZE];

// Input channel

// Output channel
single_phase_H_modulation_t hpwm;
sinv_buck_ctrl_t buck_ctrl;

// Protection module
ctl_sinv_protect_t protection;

// ADC Calibrator
adc_bias_calibrator_t adc_calibrator;
#ifdef SPECIFY_ENABLE_ADC_CALIBRATE
volatile fast_gt flag_enable_adc_calibrator = 1;
#else
volatile fast_gt flag_enable_adc_calibrator = 0;
#endif

// User commands
ctrl_gt g_p_ref_user = float2ctrl(0.0f);
ctrl_gt g_q_ref_user = float2ctrl(0.0f);
ctrl_gt g_vbus_ref_user = float2ctrl(0.0f);

ctrl_gt openloop_v_ref = float2ctrl(0.0f);
vector2_gt phasor;

//=================================================================================================
// CTL initialize routine

void ctl_init(void)
{
    //
    // stop here and wait for user start the motor controller
    //
    ctl_fast_disable_output();

    //
    // SINV current controller init objects
    //
    ctl_sinv_rc_init_t rc_init = {0};

    rc_init.fs = CONTROLLER_FREQUENCY;
    rc_init.freq_grid = CTRL_GRID_FREQUENCY;
    rc_init.v_base = CTRL_VOLTAGE_BASE;
    rc_init.i_base = CTRL_CURRENT_BASE;
    rc_init.v_bus = CTRL_DCBUS_VOLTAGE;
    rc_init.L_ac = CTRL_AC_INDUCTANCE;
    rc_init.R_ac = CTRL_AC_RESISTANCE;
    rc_init.current_loop_bw = SINV_CURRENT_LOOP_BANDWIDTH_HZ;
    rc_init.fdrc_min_freq = CTRL_FDRC_MIN_FREQ;
    rc_init.fdrc_gain = SINV_FDRC_LEARNING_GAIN;
    rc_init.fdrc_q_fc = SINV_FDRC_Q_FILTER_HZ;
    rc_init.fdrc_lead_steps = SINV_FDRC_LEAD_STEPS;
    rc_init.err_threshold = SINV_FDRC_FREEZE_ERROR_PU;

    ctl_auto_tuning_sinv_rc(&rc_init);
    ctl_init_sinv_rc_core(&rc_core, &rc_init, fdrc_buffer, FDRC_ARRAY_SIZE);

    // attach SIHV RC module to ADC peripheral
    ctl_attach_sinv_rc(&rc_core, &adc_v_bus.control_port, &adc_v_grid.control_port, &adc_i_ac.control_port);

    // init PLL & PQ controller
    ctl_init_single_phase_pll(&pll, CTRL_PLL_KP, CTRL_PLL_TI, CTRL_PLL_LPF_FC, CTRL_GRID_FREQUENCY,
                              CONTROLLER_FREQUENCY);
    ctl_init_sms_pq(&pq_meter, CTRL_GRID_FREQUENCY, CONTROLLER_FREQUENCY, CTRL_PQ_LPF_FC);

    // Command generator, I_max(pu), V_min(pu), P_slope(pu/s), Q_slope(pu/s)
    ctl_init_sinv_ref_gen(&ref_gen, CTRL_CURRENT_LIMIT_PU, CTRL_GRID_VMIN_PU, CTRL_P_SLEW_PU_S,
                          CTRL_Q_SLEW_PU_S, CONTROLLER_FREQUENCY);

    ctl_init_sinv_outer_loop(&outer_loop, SINV_POWER_LOOP_KP, SINV_POWER_LOOP_KI,
        SINV_DC_BUS_LOOP_KP, SINV_DC_BUS_LOOP_KI, SINV_OUTER_LOOP_FREQUENCY_HZ,
        CONTROLLER_FREQUENCY, SINV_OUTER_LOOP_POWER_LIMIT_PU);

    // freerun angle reference generator, 50Hz, [0, 1] range for pu angle
    ctl_init_ramp_generator_via_freq(&rg, CONTROLLER_FREQUENCY, CTRL_GRID_FREQUENCY, 1, 0);

    //
    // init H PWM modulator
    //
    ctl_init_single_phase_H_modulation(&hpwm, CTRL_PWM_CMP_MAX + 1, CTRL_PWM_DEADBAND_CMP,
                                       float2ctrl(CTRL_CURRENT_DB_PU));
    ctl_init_sinv_buck(&buck_ctrl);
    //hpwm.flag_enable_dbcomp = 1; // 开启死区补偿

    //
    // init and config CiA402 standard state machine
    //
    init_cia402_state_machine(&cia402_sm);
    cia402_sm.minimum_transit_delay[3] = SINV_CIA402_OPERATION_ENABLE_DELAY_MS;

    //
    // init and config Protection module
    //
    ctl_sinv_prot_init_t prot_init = {0};
    prot_init.error_mask = SINV_PROT_BIT_HW_TZ | SINV_PROT_BIT_DC_OVP_FAST | SINV_PROT_BIT_AC_OCP_FAST;
#if BUILD_LEVEL != 5
    /* During passive-rectifier takeover Vgrid/Vdc can legitimately demand
       more than one PU before the boost stage raises the DC link. */
    prot_init.error_mask |= SINV_PROT_BIT_CTRL_DIVERGE;
#endif
    prot_init.warning_mask = SINV_PROT_BIT_AC_OVP_RMS | SINV_PROT_BIT_AC_UVP_RMS | SINV_PROT_BIT_PLL_FREQ_ERR;
    // Protection inputs are per-unit because they are fed by adc_channel outputs.
    prot_init.v_bus_max = CTRL_PROT_VBUS_MAX / CTRL_VOLTAGE_BASE;
    prot_init.i_ac_max = CTRL_PROT_IAC_PEAK_MAX / CTRL_CURRENT_BASE;
    prot_init.v_ctrl_max = CTRL_PROT_VCTRL_MAX_PU;
    prot_init.v_ac_rms_max = 1.2f * CTRL_GRID_VOLTAGE_RMS / CTRL_VOLTAGE_BASE;
    prot_init.v_ac_rms_min = 0.8f * CTRL_GRID_VOLTAGE_RMS / CTRL_VOLTAGE_BASE;
    prot_init.freq_grid_nom = CTRL_GRID_FREQUENCY;
    prot_init.freq_dev_max = 1.0f;
    prot_init.i_ac_rated_rms = CTRL_RATED_CURRENT_RMS / CTRL_CURRENT_BASE;
    ctl_init_sinv_protect(&protection, &prot_init);

    //
    // init ADC Calibrator
    //
    ctl_init_adc_calibrator(&adc_calibrator, 20, 0.707f, CONTROLLER_FREQUENCY);
    if (flag_enable_adc_calibrator)
    {
        ctl_enable_adc_calibrator(&adc_calibrator);
    }

    //
    // incremental compilation configuration
    //

    openloop_v_ref = float2ctrl(SINV_LEVEL1_VOLTAGE_REF_PU);
#if BUILD_LEVEL == 3
    g_p_ref_user = float2ctrl(SINV_LEVEL3_ACTIVE_POWER_REF_PU);
    g_q_ref_user = float2ctrl(SINV_LEVEL3_REACTIVE_POWER_REF_PU);
#elif BUILD_LEVEL == 4
    g_p_ref_user = float2ctrl(SINV_LEVEL4_ACTIVE_POWER_REF_PU);
    g_q_ref_user = float2ctrl(0.0f);
#elif BUILD_LEVEL == 5
    g_vbus_ref_user = float2ctrl(SINV_DC_BUS_REF_V / CTRL_VOLTAGE_BASE);
#endif
    rc_core.flag_enable_fdrc = 0;
#if BUILD_LEVEL >= 2 && defined(SINV_ENABLE_GRID_VOLTAGE_FEEDFORWARD)
    rc_core.flag_enable_lead_comp = 1;
#else
    rc_core.flag_enable_lead_comp = 0;
#endif
}

//=================================================================================================
// CTL endless loop routine

void ctl_mainloop(void)
{
    // gmp_base_loop() may spin much faster than 1 kHz. Run state-machine and
    // background control policy exactly once per millisecond control tick.
    static time_gt last_tick = (time_gt)-1;
    time_gt current_tick = gmp_base_get_ctrl_tick();
    if (current_tick == last_tick)
        return;
    last_tick = current_tick;

    cia402_dispatch(&cia402_sm);

#if (BUILD_LEVEL >= 2) && defined(SINV_ENABLE_REPETITIVE_CONTROL)
    if (cia402_sm.state_word.bits.operation_enabled &&
        gmp_base_time_sub(current_tick, cia402_sm.entry_state_tick) > SINV_FDRC_ENABLE_DELAY_MS)
    {
        rc_core.flag_enable_fdrc = 1;
    }
    else
    {
        rc_core.flag_enable_fdrc = 0;
    }
#endif
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
    return mtr_ctrl.isr_tick / ((uint32_t)CONTROLLER_FREQUENCY / 1000);
}
#endif // defined ENABLE_GMP_DL_PIL_SIM

//=================================================================================================
// Controller Tasks

gmp_task_status_t tsk_protect(gmp_task_t* tsk)
{
    GMP_UNUSED_VAR(tsk);

    // PLL magnitude and the SOGI current vector are peak quantities in PU.
    const ctrl_gt inv_sqrt2 = float2ctrl(0.70710678f);
    ctrl_gt v_rms_pu = ctl_mul(ctl_abs(pll.v_mag), inv_sqrt2);
    ctrl_gt i_peak_pu = ctl_sqrt(ctl_mul(pq_meter.i_ab.dat[phase_alpha], pq_meter.i_ab.dat[phase_alpha]) +
                                 ctl_mul(pq_meter.i_ab.dat[phase_beta], pq_meter.i_ab.dat[phase_beta]));
    ctrl_gt i_rms_pu = ctl_mul(i_peak_pu, inv_sqrt2);
    ctrl_gt pll_freq_hz = ctl_mul(pll.frequency, float2ctrl(CTRL_GRID_FREQUENCY));

    ctl_task_sinv_protect_slow(&protection, v_rms_pu, pll_freq_hz, float2ctrl(0.0f), i_rms_pu);

    if (protection.active_errors != 0)
    {
        cia402_fault_request(&cia402_sm);
    }

    return GMP_TASK_DONE;
}

//=================================================================================================
// CiA402 default callback routine

//
// Check if PLL is locked.
// return 1 if PLL is locked, 0 if PLL hasn't locked
//
fast_gt ctl_check_pll_locked(void)
{
#if BUILD_LEVEL < 3
    // Bench/open-loop build levels intentionally use the free-running angle.
    return 1;
#else
    // 准入条件：
    // 1. 电网电压幅值在 0.8pu ~ 1.2pu 之间 (防止断路器未闭合或严重欠压)
    // 2. PLL 内部频率误差必须小于系统设定的容忍度 (例如 0.005 PU)
    ctrl_gt v_mag_pu = ctl_abs(pll.v_mag);
    ctrl_gt f_err_abs = ctl_abs(pll.freq_error);

    if ((v_mag_pu > float2ctrl(0.8f)) && (v_mag_pu < float2ctrl(1.2f)))
    {
        if (f_err_abs < CTRL_SPLL_EPSILON)
        {
            return 1;
        }
    }
    return 0;
#endif
}

fast_gt ctl_exec_dc_voltage_ready(void)
{
    ctrl_gt v_bus = adc_v_bus.control_port.value;
    return (v_bus >= float2ctrl(CTRL_DCBUS_READY_MIN / CTRL_VOLTAGE_BASE)) &&
           (v_bus <= float2ctrl(CTRL_DCBUS_READY_MAX / CTRL_VOLTAGE_BASE));
}

fast_gt ctl_check_compliance(void)
{
    if (protection.active_errors != 0)
        return 0;

#if BUILD_LEVEL >= 3
    /* Entry uses the tight PLL lock criterion.  Once connected, use a wider
       ride-through window so normal sample jitter cannot create an immediate
       CiA402 fault; the slow protection nodes still supervise frequency and RMS. */
    static uint16_t compliance_bad_ms = 0;
    ctrl_gt v_mag = ctl_abs(pll.v_mag);
    fast_gt valid = (v_mag > float2ctrl(0.7f)) && (v_mag < float2ctrl(1.3f)) &&
                    (pll.frequency > float2ctrl(0.9f)) && (pll.frequency < float2ctrl(1.1f));
    if (valid) {
        compliance_bad_ms = 0;
        return 1;
    }
    if (compliance_bad_ms < 100U)
        compliance_bad_ms++;
    return compliance_bad_ms < 100U;
#else
    return 1;
#endif
}

fast_gt ctl_fault_recover_routine(void)
{
    ctl_reset_sinv_protect(&protection);
    clear_all_controllers();
    return 1;
}

//
// ADC Auto calibrate routine
//
fast_gt ctl_exec_adc_calibration(void)
{
    if (flag_enable_adc_calibrator)
    {
        if (ctl_is_adc_calibrator_cmpt(&adc_calibrator) && ctl_is_adc_calibrator_result_valid(&adc_calibrator))
        {
            adc_i_ac.bias += ctl_get_adc_calibrator_result(&adc_calibrator);
            flag_enable_adc_calibrator = 0;
            clear_all_controllers();
        }

        // ADC calibrate is not complete
        return 0;
    }

    // skip calibrate routine
    return 1;
}

void clear_all_controllers(void)
{
    ctl_clear_single_phase_pll(&pll);
    ctl_clear_sms_pq(&pq_meter);
    ctl_clear_sinv_rc_core(&rc_core);
    ctl_clear_sinv_ref_gen(&ref_gen);
    ctl_clear_sinv_outer_loop(&outer_loop);
    ctl_clear_single_phase_H_modulation(&hpwm);
    ctl_clear_sinv_buck(&buck_ctrl);
}

void ctl_init_sinv_buck(sinv_buck_ctrl_t* buck)
{
    ctl_init_pid(&buck->voltage_pid, SINV_BUCK_VOLTAGE_LOOP_KP, SINV_BUCK_VOLTAGE_LOOP_KI, 0.0f,
                 SINV_BUCK_VOLTAGE_LOOP_FREQUENCY_HZ);
    ctl_set_pid_limit(&buck->voltage_pid, float2ctrl(SINV_BUCK_CURRENT_LIMIT_A / CTRL_CURRENT_BASE),
                      float2ctrl(0.0f));
    ctl_set_pid_int_limit(&buck->voltage_pid, float2ctrl(SINV_BUCK_CURRENT_LIMIT_A / CTRL_CURRENT_BASE),
                          float2ctrl(0.0f));

    ctl_init_pid(&buck->current_pid, SINV_BUCK_CURRENT_LOOP_KP, SINV_BUCK_CURRENT_LOOP_KI, 0.0f,
                 CONTROLLER_FREQUENCY);
    ctl_set_pid_limit(&buck->current_pid, float2ctrl(SINV_BUCK_DUTY_TRIM_LIMIT),
                      float2ctrl(-SINV_BUCK_DUTY_TRIM_LIMIT));
    ctl_set_pid_int_limit(&buck->current_pid, float2ctrl(SINV_BUCK_DUTY_TRIM_LIMIT),
                          float2ctrl(-SINV_BUCK_DUTY_TRIM_LIMIT));

    buck->v_ref_target = float2ctrl(SINV_BUCK_OUTPUT_REF_V / CTRL_VOLTAGE_BASE);
    buck->v_ref_step = float2ctrl(SINV_BUCK_VREF_SLEW_V_S / CTRL_VOLTAGE_BASE / CONTROLLER_FREQUENCY);
    buck->startup_delay_count = (uint32_t)((float)SINV_BUCK_START_DELAY_MS * (float)CONTROLLER_FREQUENCY / 1000.0f);
    ctl_clear_sinv_buck(buck);
}

void ctl_clear_sinv_buck(sinv_buck_ctrl_t* buck)
{
    ctl_clear_pid(&buck->voltage_pid);
    ctl_clear_pid(&buck->current_pid);
    buck->v_ref = float2ctrl(0.0f);
    buck->v_out_lpf = float2ctrl(0.0f);
    buck->i_ref = float2ctrl(0.0f);
    buck->v_in_ff = float2ctrl(0.0f);
    buck->duty_ff = float2ctrl(0.0f);
    buck->duty_trim = float2ctrl(0.0f);
    buck->duty_cmd = float2ctrl(0.0f);
    buck->duty = float2ctrl(0.0f);
    buck->startup_counter = 0U;
    buck->voltage_loop_counter = 0U;
    buck->pwm_cmp = 0U;
    buck->flag_enable = 0;
}

pwm_gt ctl_step_sinv_buck(sinv_buck_ctrl_t* buck, ctrl_gt v_in, ctrl_gt v_out, ctrl_gt i_l, fast_gt enable)
{
    if (!enable)
    {
        ctl_clear_sinv_buck(buck);
        return 0U;
    }

    if (buck->startup_counter < buck->startup_delay_count)
    {
        buck->startup_counter++;
        buck->pwm_cmp = 0U;
        return 0U;
    }

    /* Buck 输出电压采样存在开关纹波/采样碎纹；这里用固定软件低通，
       只给电压外环使用，不作为 SDPE 参数暴露，避免再引入一项调参量。 */
    if (!buck->flag_enable)
    {
        buck->v_out_lpf = v_out;
        buck->flag_enable = 1;
    }
    else
    {
        buck->v_out_lpf += ctl_mul(float2ctrl(0.02f), v_out - buck->v_out_lpf);
    }

    buck->v_ref += buck->v_ref_step;
    if (buck->v_ref > buck->v_ref_target)
        buck->v_ref = buck->v_ref_target;

    const uint32_t voltage_loop_div =
        (uint32_t)((float)CONTROLLER_FREQUENCY / (float)SINV_BUCK_VOLTAGE_LOOP_FREQUENCY_HZ);
    if ((buck->voltage_loop_counter == 0U) || (voltage_loop_div <= 1U))
    {
        buck->i_ref = ctl_step_pid_par(&buck->voltage_pid, buck->v_ref - buck->v_out_lpf);
    }
    buck->voltage_loop_counter++;
    if ((voltage_loop_div > 1U) && (buck->voltage_loop_counter >= voltage_loop_div))
        buck->voltage_loop_counter = 0U;

    ctrl_gt v_in_safe = v_in;
    if (v_in_safe < float2ctrl(1.0f / CTRL_VOLTAGE_BASE))
        v_in_safe = float2ctrl(1.0f / CTRL_VOLTAGE_BASE);

    if (buck->v_in_ff <= float2ctrl(0.0f))
        buck->v_in_ff = v_in_safe;
    else
        buck->v_in_ff += ctl_mul(float2ctrl(SINV_BUCK_VIN_FF_LPF_ALPHA), v_in_safe - buck->v_in_ff);

    ctrl_gt duty_nominal = ctl_div(buck->v_ref, float2ctrl(CTRL_DCBUS_VOLTAGE / CTRL_VOLTAGE_BASE));
    duty_nominal = ctl_sat(duty_nominal, float2ctrl(SINV_BUCK_DUTY_MAX), float2ctrl(SINV_BUCK_DUTY_MIN));

    ctrl_gt duty_vin_ff = ctl_div(buck->v_ref, buck->v_in_ff);
    duty_vin_ff = ctl_sat(duty_vin_ff, float2ctrl(SINV_BUCK_DUTY_MAX), float2ctrl(SINV_BUCK_DUTY_MIN));

    buck->duty_ff = duty_nominal + ctl_mul(float2ctrl(SINV_BUCK_DUTY_FF_GAIN), duty_vin_ff - duty_nominal);
    buck->duty_ff = ctl_sat(buck->duty_ff, float2ctrl(SINV_BUCK_DUTY_MAX), float2ctrl(SINV_BUCK_DUTY_MIN));

    buck->duty_trim = ctl_step_pid_par(&buck->current_pid, buck->i_ref - i_l);
    buck->duty_cmd = buck->duty_ff + buck->duty_trim;

    ctrl_gt duty_upper = buck->duty_ff + float2ctrl(SINV_BUCK_DUTY_FF_MARGIN);
    duty_upper = ctl_sat(duty_upper, float2ctrl(SINV_BUCK_DUTY_MAX), float2ctrl(SINV_BUCK_DUTY_MIN));

    buck->duty = ctl_sat(buck->duty_cmd, duty_upper, float2ctrl(SINV_BUCK_DUTY_MIN));
    buck->pwm_cmp = pwm_sat(ctrl2float(buck->duty) * (float)(CTRL_PWM_CMP_MAX + 1), CTRL_PWM_CMP_MAX + 1, 0);
    return buck->pwm_cmp;
}

void ctl_enable_pwm(void)
{
    ctl_fast_enable_output();
}

void ctl_disable_pwm(void)
{
    ctl_fast_disable_output();
}
