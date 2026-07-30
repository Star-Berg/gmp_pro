/**
 * @file ctl_main.cpp
 * @author Javnson (javnson@zju.edu.cn)
 * @brief
 * @version 0.1
 * @date 2024-09-30
 *
 * @copyright Copyright GMP(c) 2024
 *
 */

#include <xplt.peripheral.h>

//=================================================================================================
// include Necessary control modules

#include <ctl/component/interface/adc_channel.h>
#include <ctl/component/interface/pwm_channel.h>

#include <ctl/component/digital_power/inv/gfl_core.h>
#include <ctl/component/digital_power/inv/gfl_pq_ctrl.h>
#include <ctl/component/digital_power/inv/inv_hcm.h>
#include <ctl/component/digital_power/inv/inv_neg_ctrl.h>

#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7
#include "ctl_level67_voltage.h"
#endif

#include <ctl/component/interface/spwm_modulator.h>

#include <ctl/framework/cia402_state_machine.h>

#ifndef _FILE_CTL_MAIN_H_
#define _FILE_CTL_MAIN_H_

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

//=================================================================================================
// extern controller modules

// System framework
extern cia402_sm_t cia402_sm;

// Control Law Core
extern gfl_inv_ctrl_init_t gfl_init;
extern gfl_inv_ctrl_t inv_ctrl;
extern gfl_pq_ctrl_t pq_ctrl;
extern inv_neg_ctrl_init_t gfl_neg_init;
extern inv_neg_ctrl_t neg_current_ctrl;
#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7
extern gfl_level67_voltage_ctrl_t voltage_ctrl;
#endif

// Input channel

// Output channel
#if defined USING_NPC_MODULATOR
extern npc_modulator_t spwm;
#else
extern spwm_modulator_t spwm;
#endif // USING_NPC_MODULATOR

// Protection module

// ADC Calibrator
extern adc_bias_calibrator_t adc_calibrator;
extern volatile fast_gt flag_enable_adc_calibrator;
extern volatile fast_gt index_adc_calibrator;
extern uint32_t pq_loop_tick;

// Local keyboard operator request and selected off-grid operating profile.
extern volatile fast_gt ctl_user_run_request;
extern volatile uint16_t ctl_output_frequency_hz;
extern volatile uint16_t ctl_output_frequency_request_hz;
extern volatile uint16_t ctl_dc_bus_voltage_setting_v;
extern volatile uint16_t ctl_dc_bus_voltage_request_v;
extern volatile ctrl_gt ctl_dc_bus_feedforward_gain;

void ctl_set_run_request(fast_gt enable);
void ctl_request_output_frequency_hz(uint16_t frequency_hz);
void ctl_apply_output_frequency_request(void);
void ctl_request_dc_bus_voltage_v(uint16_t dc_bus_voltage_v);
void ctl_apply_dc_bus_voltage_request(void);

// User commands

#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7
GMP_STATIC_INLINE ctrl_gt ctl_calc_dc_bus_feedforward_gain(void)
{
#if GFL_LEVEL67_ENABLE_DCBUS_FEEDFORWARD != 0
    ctrl_gt dc_bus_safe = inv_ctrl.filter_udc.out;
    ctrl_gt dc_bus_min = float2ctrl(GFL_LEVEL67_DCBUS_FEEDFORWARD_MIN_V / CTRL_VOLTAGE_BASE);
    ctrl_gt nominal_dc_bus = float2ctrl(CTRL_DCBUS_VOLTAGE / CTRL_VOLTAGE_BASE);
    ctrl_gt gain;

    if (dc_bus_safe < dc_bus_min)
        dc_bus_safe = dc_bus_min;

    gain = ctl_div(nominal_dc_bus, dc_bus_safe);
    gain = ctl_sat(gain, float2ctrl(GFL_LEVEL67_DCBUS_FEEDFORWARD_MAX_GAIN),
                   float2ctrl(GFL_LEVEL67_DCBUS_FEEDFORWARD_MIN_GAIN));
    return gain;
#else
    return float2ctrl(1.0f);
#endif
}

GMP_STATIC_INLINE void ctl_apply_dc_bus_feedforward_to_modulator(void)
{
    ctrl_gt target_gain = ctl_calc_dc_bus_feedforward_gain();
    ctrl_gt gain_delta =
        ctl_mul(float2ctrl(GFL_LEVEL67_DCBUS_FEEDFORWARD_LPF_ALPHA),
                target_gain - ctl_dc_bus_feedforward_gain);
    ctrl_gt gain_slew_step = float2ctrl(GFL_LEVEL67_DCBUS_FEEDFORWARD_SLEW_STEP);

    if (gain_delta > gain_slew_step)
        gain_delta = gain_slew_step;
    else if (gain_delta < -gain_slew_step)
        gain_delta = -gain_slew_step;

    ctl_dc_bus_feedforward_gain += gain_delta;

    spwm.vab0_out.dat[phase_A] = ctl_mul(spwm.vab0_out.dat[phase_A], ctl_dc_bus_feedforward_gain);
    spwm.vab0_out.dat[phase_B] = ctl_mul(spwm.vab0_out.dat[phase_B], ctl_dc_bus_feedforward_gain);
    spwm.vab0_out.dat[phase_0] = ctl_mul(spwm.vab0_out.dat[phase_0], ctl_dc_bus_feedforward_gain);
}
#endif

//=================================================================================================
// controller process

// periodic callback function things.
GMP_STATIC_INLINE void ctl_dispatch(void)
{
    // ADC calibrator routine
    if (flag_enable_adc_calibrator)
    {
        if (index_adc_calibrator == 13)
            ctl_step_adc_calibrator(&adc_calibrator, idc.control_port.value);
        else if (index_adc_calibrator == 12)
            ctl_step_adc_calibrator(&adc_calibrator, udc.control_port.value);
        else if (index_adc_calibrator <= 11 && index_adc_calibrator >= 9)
            ctl_step_adc_calibrator(&adc_calibrator, uuvw.control_port.value.dat[index_adc_calibrator - 9]);
        else if (index_adc_calibrator <= 8 && index_adc_calibrator >= 6)
            ctl_step_adc_calibrator(&adc_calibrator, vabc.control_port.value.dat[index_adc_calibrator - 6]);
        else if (index_adc_calibrator <= 5 && index_adc_calibrator >= 3)
            ctl_step_adc_calibrator(&adc_calibrator, iabc.control_port.value.dat[index_adc_calibrator - 3]);
        else if (index_adc_calibrator <= 2)
            ctl_step_adc_calibrator(&adc_calibrator, iuvw.control_port.value.dat[index_adc_calibrator]);
    }

    // normal controller routine
    else
    {
        // run controller body
        ctl_step_gfl_inv_ctrl(&inv_ctrl);

#if BUILD_LEVEL == 6
        if (inv_ctrl.flag_enable_system)
        {
            ctl_step_level6_voltage(&voltage_ctrl, &inv_ctrl.vdq);
            ctl_set_gfl_inv_current(&inv_ctrl, voltage_ctrl.idq_ref.dat[phase_d],
                                    voltage_ctrl.idq_ref.dat[phase_q]);
            ctl_step_level67_positive_current(&inv_ctrl, &inv_ctrl.idq, &voltage_ctrl.vdq_feedback);
#if GFL_LEVEL67_ENABLE_NEG_SEQ_CTRL != 0
            ctl_step_neg_inv_ctrl(&neg_current_ctrl);
#else
            ctl_vector2_clear(&neg_current_ctrl.vab_out);
#endif
        }
        else
        {
            ctl_vector2_clear(&neg_current_ctrl.vab_out);
        }
#elif BUILD_LEVEL == 7
        if (inv_ctrl.flag_enable_system)
        {
            ctl_step_level7_voltage(&voltage_ctrl, (ctl_vector2_t*)&inv_ctrl.vab0,
                                    (ctl_vector2_t*)&inv_ctrl.iab0, &inv_ctrl.phasor);
            ctl_set_gfl_inv_current(&inv_ctrl, voltage_ctrl.idq_ref.dat[phase_d],
                                    voltage_ctrl.idq_ref.dat[phase_q]);
            ctl_step_level67_positive_current(&inv_ctrl, &voltage_ctrl.current_seq.pos_decoupled,
                                              &voltage_ctrl.voltage_seq.pos_dc);
#if GFL_LEVEL67_ENABLE_NEG_SEQ_CTRL != 0
            ctl_step_neg_inv_ctrl_dq(&neg_current_ctrl, &voltage_ctrl.current_seq.neg_decoupled,
                                     &voltage_ctrl.voltage_seq.neg_dc);
#else
            ctl_vector2_clear(&neg_current_ctrl.vab_out);
#endif
        }
        else
        {
            ctl_vector2_clear(&neg_current_ctrl.vab_out);
        }
#else
        ctl_step_neg_inv_ctrl(&neg_current_ctrl);
#endif

        // Run the P/Q outer loop at its own lower rate. The current loop keeps
        // executing every ISR and consumes the most recent current reference.
        ++pq_loop_tick;
        if (pq_loop_tick >= GFL_PQ_LOOP_DIVIDER)
        {
            pq_loop_tick = 0;
            ctl_step_gfl_pq(&pq_ctrl);

            if (pq_ctrl.flag_enable)
            {
                ctl_set_gfl_inv_current(&inv_ctrl, pq_ctrl.idq_set_out.dat[phase_d],
                                        pq_ctrl.idq_set_out.dat[phase_q]);
            }
        }

        // mix all output
        spwm.vab0_out.dat[phase_A] = inv_ctrl.vab0_out.dat[phase_A] + neg_current_ctrl.vab_out.dat[phase_A];
        spwm.vab0_out.dat[phase_B] = inv_ctrl.vab0_out.dat[phase_B] + neg_current_ctrl.vab_out.dat[phase_B];
        spwm.vab0_out.dat[phase_0] = inv_ctrl.vab0_out.dat[phase_0];

#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7
        ctl_apply_dc_bus_feedforward_to_modulator();
#endif

        // modulation
#if defined USING_NPC_MODULATOR
        ctl_step_npc_modulator(&spwm);
#else
        ctl_step_spwm_modulator(&spwm);
#endif // USING_NPC_MODULATOR
    }
}

#ifdef __cplusplus
}
#endif // _cplusplus

#endif // _FILE_CTL_MAIN_H_
