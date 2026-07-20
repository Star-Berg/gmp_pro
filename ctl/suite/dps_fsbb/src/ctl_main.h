/**
 * @file ctl_main.h
 * @brief Top-level controller interface for the four-switch buck-boost converter.
 */

#ifndef _FILE_CTRL_MAIN_H_
#define _FILE_CTRL_MAIN_H_

#ifndef SDPE_FSBB_SETTINGS_HEADER
#define SDPE_FSBB_SETTINGS_HEADER <sdpe_dps_fsbb_iris_settings.h>
#endif
#include SDPE_FSBB_SETTINGS_HEADER
#include <core/pm/function_scheduler.h>
#include <core/dev/pil_core.h>
#include <ctl/framework/cia402_state_machine.h>
#include <ctl/component/interface/adc_channel.h>
#include <ctl/component/digital_power/dcdc/dcdc_core.h>
#include <ctl/component/digital_power/dcdc/fsbb.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern cia402_sm_t cia402_sm;
extern ctl_dcdc_core_t dcdc_core;

extern adc_channel_t adc_v_in;
extern adc_channel_t adc_v_out;
extern adc_channel_t adc_i_L;
extern adc_channel_t adc_i_load;
extern fsbb_modulator_t fsbb_mod;

extern adc_bias_calibrator_t adc_calibrator;
extern volatile fast_gt flag_enable_adc_calibrator;
extern volatile fast_gt index_adc_calibrator;

extern ctrl_gt g_v_out_ref_user;
extern ctrl_gt g_i_out_ref_user;
/* Legacy name retained for existing CAN/SDPE/debug integrations. */
#define g_i_limit_user g_i_out_ref_user
extern ctrl_gt v_req;

typedef enum _tag_fsbb_control_mode_t
{
    FSBB_CTRL_CV = 1,
    FSBB_CTRL_CC = 2
} fsbb_control_mode_t;

typedef enum _tag_fsbb_regulation_state_t
{
    FSBB_REG_DISABLED = 0,
    FSBB_REG_CV,
    FSBB_REG_CC,
    FSBB_REG_FAULT
} fsbb_regulation_state_t;

typedef enum _tag_fsbb_fault
{
    FSBB_FAULT_NONE = 0,
    FSBB_FAULT_VIN_UNDERVOLTAGE = 1U << 0,
    FSBB_FAULT_VIN_OVERVOLTAGE = 1U << 1,
    FSBB_FAULT_VOUT_OVERVOLTAGE = 1U << 2,
    FSBB_FAULT_IL_POSITIVE_OVERCURRENT = 1U << 3,
    FSBB_FAULT_IL_NEGATIVE_OVERCURRENT = 1U << 4,
    FSBB_FAULT_IOUT_OVERCURRENT = 1U << 5
} fsbb_fault_t;

extern volatile uint16_t g_fsbb_faults;
extern volatile fast_gt g_fsbb_output_enabled;
extern volatile fsbb_control_mode_t g_fsbb_mode_request;
extern volatile fsbb_control_mode_t g_fsbb_mode_active;
extern volatile fsbb_regulation_state_t g_fsbb_regulation_state;

extern ctl_pid_t output_current_pid;
extern ctl_slope_limiter_t iout_ref_ramp;
extern ctrl_gt g_fsbb_i_ref_cv;
extern ctrl_gt g_fsbb_i_ref_cc;
extern ctrl_gt g_fsbb_i_L_ref_selected;

void ctl_init(void);
void ctl_mainloop(void);
void ctl_enable_pwm(void);
void ctl_disable_pwm(void);
void clear_all_controllers(void);
/**
 * @brief Request a latched-fault reset after all real-time fault conditions clear.
 * @return 1 when the reset command is accepted; 0 while a fault is still active
 *         or there is no latched fault to reset.
 */
fast_gt ctl_request_fault_reset(void);
fast_gt ctl_request_control_mode(fsbb_control_mode_t mode);
ctrl_gt ctl_step_fsbb_cv_cc(void);
gmp_task_status_t tsk_protect(gmp_task_t* tsk);

/** Execute one control sample after the platform input callback has run. */
GMP_STATIC_INLINE void ctl_dispatch(void)
{
#if defined SPECIFY_ENABLE_ADC_CALIBRATE
    if (flag_enable_adc_calibrator)
    {
        if (index_adc_calibrator == 0)
            ctl_step_adc_calibrator(&adc_calibrator, adc_i_L.control_port.value);
#if defined FSBB_ENABLE_IOUT_SAMPLE
        else if (index_adc_calibrator == 1)
            ctl_step_adc_calibrator(&adc_calibrator, adc_i_load.control_port.value);
#endif
        return;
    }
#endif

    if (g_fsbb_faults != FSBB_FAULT_NONE)
    {
        g_fsbb_regulation_state = FSBB_REG_FAULT;
        return;
    }

#if (BUILD_LEVEL == 3)
    if (g_fsbb_output_enabled == 0)
    {
        g_fsbb_regulation_state = FSBB_REG_DISABLED;
        return;
    }
#endif

#if (BUILD_LEVEL == 1)
    dcdc_core.mode = CTL_DCDC_MODE_OPENLOOP;
    dcdc_core.v_target = float2ctrl(FSBB_OPEN_LOOP_VOLTAGE_COMMAND / CTRL_VOLTAGE_BASE);
    v_req = ctl_step_dcdc_open_loop(&dcdc_core);
#elif (BUILD_LEVEL == 2)
    dcdc_core.mode = CTL_DCDC_MODE_CURRENTLOOP;
    dcdc_core.i_target = ctl_sat(g_i_limit_user,
                                 float2ctrl(FSBB_OUTPUT_CURRENT_LIM / CTRL_CURRENT_BASE),
                                 float2ctrl(0.0f));
    v_req = ctl_step_dcdc_current_loop(&dcdc_core);
#elif (BUILD_LEVEL == 3)
    dcdc_core.mode = CTL_DCDC_MODE_VOLTAGELOOP;
    v_req = ctl_step_fsbb_cv_cc();
#endif

    ctl_step_fsbb_modulator(&fsbb_mod, v_req, adc_v_in.control_port.value);
}

#ifdef __cplusplus
}
#endif

#endif // _FILE_CTRL_MAIN_H_
