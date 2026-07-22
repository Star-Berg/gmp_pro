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

#include "offgrid_voltage_ctrl.h"

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
extern offgrid_voltage_ctrl_init_t offgrid_voltage_init;
extern offgrid_voltage_ctrl_t offgrid_voltage_ctrl;

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

// BUILD_LEVEL 6 runtime commands. Voltage is line-to-line RMS.
void ctl_set_level6_voltage_rms(parameter_gt line_voltage_rms_v);
void ctl_set_level6_frequency_hz(parameter_gt frequency_hz);

// User commands

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

#if BUILD_LEVEL >= 3 && BUILD_LEVEL <= 5
        ctl_step_neg_inv_ctrl(&neg_current_ctrl);
#endif

#if BUILD_LEVEL == 5
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
#endif // BUILD_LEVEL == 5

#if BUILD_LEVEL == 6
        // Stand-alone voltage source: bypass PLL, negative-sequence and P/Q
        // paths, then send the stationary-frame voltage controller directly
        // to the SPWM modulator.
        ctl_step_offgrid_voltage_ctrl(&offgrid_voltage_ctrl, &inv_ctrl, inv_ctrl.flag_enable_system);
        spwm.vab0_out.dat[phase_alpha] = offgrid_voltage_ctrl.modulation_ab.dat[phase_alpha];
        spwm.vab0_out.dat[phase_beta] = offgrid_voltage_ctrl.modulation_ab.dat[phase_beta];
        spwm.vab0_out.dat[phase_0] = 0;
#else
        // mix all output
        spwm.vab0_out.dat[phase_A] = inv_ctrl.vab0_out.dat[phase_A] + neg_current_ctrl.vab_out.dat[phase_A];
        spwm.vab0_out.dat[phase_B] = inv_ctrl.vab0_out.dat[phase_B] + neg_current_ctrl.vab_out.dat[phase_B];
        spwm.vab0_out.dat[phase_0] = inv_ctrl.vab0_out.dat[phase_0];
#endif // BUILD_LEVEL == 6

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
