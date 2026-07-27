/**
 * @file level6_dc_offset_servo.h
 * @brief Cycle-average DC-voltage suppression integrated into the Level 6 voltage outer loop.
 *
 * The alpha/beta QPR voltage controllers have only finite gain at 0 Hz.  This
 * helper averages the alpha/beta voltage error over a complete output period,
 * integrates a small DC current-reference correction, and injects that
 * correction before the estimated-inductor-current vector limit and current
 * QPR loop.  The correction therefore remains inside the cascaded control
 * structure instead of being added directly to the final PWM command.
 */

#ifndef _FILE_PGS_INV_GFL_LEVEL6_DC_OFFSET_SERVO_H_
#define _FILE_PGS_INV_GFL_LEVEL6_DC_OFFSET_SERVO_H_

#ifndef GFL_LEVEL6_ENABLE_DC_OFFSET_SERVO
#define GFL_LEVEL6_ENABLE_DC_OFFSET_SERVO (1)
#endif

/* DC current-reference correction PU per voltage-error PU per second. */
#ifndef GFL_LEVEL6_DC_SERVO_KI_PER_S
#define GFL_LEVEL6_DC_SERVO_KI_PER_S (0.10f)
#endif

/* Ignore cycle-average voltage errors below this controller-PU magnitude. */
#ifndef GFL_LEVEL6_DC_SERVO_DEADBAND_PU
#define GFL_LEVEL6_DC_SERVO_DEADBAND_PU (0.002f)
#endif

/* Independent alpha/beta DC current-reference correction limit. */
#ifndef GFL_LEVEL6_DC_SERVO_LIMIT_PU
#define GFL_LEVEL6_DC_SERVO_LIMIT_PU (0.05f)
#endif

/* Freeze integration when the previous modulation vector approaches limit. */
#ifndef GFL_LEVEL6_DC_SERVO_MODULATION_GUARD_PU
#define GFL_LEVEL6_DC_SERVO_MODULATION_GUARD_PU (0.93f)
#endif

typedef struct _tag_level6_dc_offset_servo
{
    parameter_gt error_sum_ab[2];
    parameter_gt error_mean_ab[2];
    parameter_gt current_correction_ab[2];
    uint32_t sample_count;
    ctrl_gt angle_last_pu;
    fast_gt initialized;
} level6_dc_offset_servo_t;

GMP_STATIC_INLINE parameter_gt ctl_level6_dc_servo_clamp(parameter_gt value,
                                                          parameter_gt limit)
{
    if (value > limit)
        return limit;
    if (value < -limit)
        return -limit;
    return value;
}

GMP_STATIC_INLINE parameter_gt ctl_level6_dc_servo_abs(parameter_gt value)
{
    return value >= 0.0f ? value : -value;
}

GMP_STATIC_INLINE void ctl_clear_level6_dc_offset_servo(
    level6_dc_offset_servo_t* servo)
{
    int axis;

    for (axis = 0; axis < 2; ++axis)
    {
        servo->error_sum_ab[axis] = 0.0f;
        servo->error_mean_ab[axis] = 0.0f;
        servo->current_correction_ab[axis] = 0.0f;
    }

    servo->sample_count = 0;
    servo->angle_last_pu = float2ctrl(0.0f);
    servo->initialized = 0;
}

/**
 * @brief Execute the complete Level 6 controller with the DC servo placed in
 *        the voltage-loop output/current-reference path.
 */
GMP_STATIC_INLINE void ctl_step_offgrid_voltage_ctrl_with_dc_servo(
    offgrid_voltage_ctrl_t* ctrl,
    const gfl_inv_ctrl_t* core,
    fast_gt enabled)
{
#if GFL_LEVEL6_ENABLE_DC_OFFSET_SERVO
    static level6_dc_offset_servo_t servo;
    ctrl_gt voltage_ref_peak_pu;
    ctrl_gt voltage_step;
    ctrl_gt voltage_correction;
    ctrl_gt current_correction;
    ctrl_gt dc_bus_gain_ctrl;
    parameter_gt quantized_command;
    parameter_gt dc_bus_gain_delta;
    parameter_gt modulation_alpha;
    parameter_gt modulation_beta;
    parameter_gt modulation_magnitude_sq;
    parameter_gt integration_step;
    fast_gt cycle_wrapped;
    fast_gt command_slew_complete;
    fast_gt modulation_has_margin;
    int axis;

    gmp_base_assert(ctrl);
    gmp_base_assert(core);

    if (!enabled)
    {
        if (servo.initialized)
            ctl_clear_level6_dc_offset_servo(&servo);
        ctl_step_offgrid_voltage_ctrl(ctrl, core, enabled);
        return;
    }

    if (!ctrl->flag_was_enabled)
    {
        ctrl->voltage_ab_last.dat[phase_alpha] = core->vab0.dat[phase_alpha];
        ctrl->voltage_ab_last.dat[phase_beta] = core->vab0.dat[phase_beta];
    }
    ctrl->flag_was_enabled = 1;

    if (!servo.initialized)
    {
        ctl_clear_level6_dc_offset_servo(&servo);
        servo.angle_last_pu = ctrl->angle_pu;
        servo.initialized = 1;
    }

    /* Use the previous ISR modulation magnitude as the anti-windup guard. */
    modulation_alpha = ctrl2float(ctrl->modulation_ab.dat[phase_alpha]);
    modulation_beta = ctrl2float(ctrl->modulation_ab.dat[phase_beta]);
    modulation_magnitude_sq = modulation_alpha * modulation_alpha +
                              modulation_beta * modulation_beta;
    modulation_has_margin =
        modulation_magnitude_sq <
        (GFL_LEVEL6_DC_SERVO_MODULATION_GUARD_PU *
         GFL_LEVEL6_DC_SERVO_MODULATION_GUARD_PU);

    /* Accept direct Datalink writes while retaining the public API limits. */
    quantized_command = ctl_offgrid_quantize_parameter(
        ctrl->line_voltage_rms_cmd_v,
        GFL_LEVEL6_VOLTAGE_MIN_RMS_V,
        GFL_LEVEL6_VOLTAGE_MAX_RMS_V,
        GFL_LEVEL6_VOLTAGE_STEP_RMS_V);
    ctrl->line_voltage_rms_cmd_v = quantized_command;

    quantized_command = ctl_offgrid_quantize_parameter(
        ctrl->frequency_cmd_hz,
        GFL_LEVEL6_FREQUENCY_MIN_HZ,
        GFL_LEVEL6_FREQUENCY_MAX_HZ,
        GFL_LEVEL6_FREQUENCY_STEP_HZ);
    ctrl->frequency_cmd_hz = quantized_command;
    if (ctrl->frequency_active_hz != quantized_command)
        ctl_update_offgrid_frequency(ctrl, quantized_command);

    /* Slew the physical line RMS command to avoid an inrush-producing step. */
    voltage_step = ctrl->voltage_slew_step_v;
    if (ctrl->line_voltage_rms_active_v < ctrl->line_voltage_rms_cmd_v)
    {
        ctrl->line_voltage_rms_active_v += ctrl2float(voltage_step);
        if (ctrl->line_voltage_rms_active_v > ctrl->line_voltage_rms_cmd_v)
            ctrl->line_voltage_rms_active_v = ctrl->line_voltage_rms_cmd_v;
    }
    else if (ctrl->line_voltage_rms_active_v > ctrl->line_voltage_rms_cmd_v)
    {
        ctrl->line_voltage_rms_active_v -= ctrl2float(voltage_step);
        if (ctrl->line_voltage_rms_active_v < ctrl->line_voltage_rms_cmd_v)
            ctrl->line_voltage_rms_active_v = ctrl->line_voltage_rms_cmd_v;
    }

    command_slew_complete =
        ctl_level6_dc_servo_abs(ctrl->line_voltage_rms_active_v -
                                ctrl->line_voltage_rms_cmd_v) < 0.001f;

    /* Measured DC-bus feed-forward normalization. */
#if GFL_LEVEL6_ENABLE_DCBUS_FEEDFORWARD
    ctrl->dc_bus_voltage_meas_v =
        ctrl2float(core->filter_udc.out) * ctrl->voltage_base;

    if (ctrl->dc_bus_voltage_meas_v >= GFL_LEVEL6_DCBUS_VALID_MIN_V &&
        ctrl->dc_bus_voltage_meas_v <= GFL_LEVEL6_DCBUS_VALID_MAX_V)
    {
        ctrl->dc_bus_feedforward_target_gain =
            ctrl->dc_bus_voltage_nominal_v / ctrl->dc_bus_voltage_meas_v;
        ctrl->dc_bus_feedforward_target_gain = ctl_offgrid_clamp_parameter(
            ctrl->dc_bus_feedforward_target_gain,
            GFL_LEVEL6_DCBUS_GAIN_MIN,
            GFL_LEVEL6_DCBUS_GAIN_MAX);
    }
    else
    {
        ctrl->dc_bus_feedforward_target_gain = 1.0f;
    }
#else
    ctrl->dc_bus_voltage_meas_v = ctrl->dc_bus_voltage_nominal_v;
    ctrl->dc_bus_feedforward_target_gain = 1.0f;
#endif

    dc_bus_gain_delta =
        ctrl->dc_bus_feedforward_target_gain - ctrl->dc_bus_feedforward_gain;
    if (dc_bus_gain_delta > ctrl->dc_bus_gain_slew_step)
        ctrl->dc_bus_feedforward_gain += ctrl->dc_bus_gain_slew_step;
    else if (dc_bus_gain_delta < -ctrl->dc_bus_gain_slew_step)
        ctrl->dc_bus_feedforward_gain -= ctrl->dc_bus_gain_slew_step;
    else
        ctrl->dc_bus_feedforward_gain = ctrl->dc_bus_feedforward_target_gain;

    dc_bus_gain_ctrl = float2ctrl(ctrl->dc_bus_feedforward_gain);

    /* Keep phase continuous when the frequency command changes. */
    ctrl->angle_pu += ctrl->angle_step_pu;
    if (ctrl->angle_pu >= float2ctrl(1.0f))
        ctrl->angle_pu -= float2ctrl(1.0f);
    cycle_wrapped = ctrl->angle_pu < servo.angle_last_pu;
    servo.angle_last_pu = ctrl->angle_pu;
    ctl_set_phasor_via_angle(ctrl->angle_pu, &ctrl->phasor);

    /* V_phase,peak = sqrt(2/3) * V_line,rms. */
    voltage_ref_peak_pu = float2ctrl(
        0.816496580927726f * ctrl->line_voltage_rms_active_v *
        GFL_LEVEL6_VOLTAGE_REFERENCE_GAIN / ctrl->voltage_base);
    ctrl->voltage_ref_ab.dat[phase_alpha] =
        ctl_mul(voltage_ref_peak_pu, ctrl->phasor.dat[phasor_cos]);
    ctrl->voltage_ref_ab.dat[phase_beta] =
        ctl_mul(voltage_ref_peak_pu, ctrl->phasor.dat[phasor_sin]);

    /* Estimate capacitor current, then iL = iLoad + iC. */
    for (axis = 0; axis < 2; ++axis)
    {
        ctl_offgrid_step_capacitor_current_estimate(
            ctrl, axis, core->vab0.dat[axis]);
        ctrl->inductor_current_est_ab.dat[axis] =
            core->iab0.dat[axis] +
            ctrl->capacitor_current_est_ab.dat[axis];
    }

    /* Analytic capacitor-current feed-forward for the sinusoidal reference. */
    ctrl->capacitor_current_ref_ab.dat[phase_alpha] = -ctl_mul(
        ctl_mul(ctrl->capacitor_reference_gain, voltage_ref_peak_pu),
        ctrl->phasor.dat[phasor_sin]);
    ctrl->capacitor_current_ref_ab.dat[phase_beta] = ctl_mul(
        ctl_mul(ctrl->capacitor_reference_gain, voltage_ref_peak_pu),
        ctrl->phasor.dat[phasor_cos]);

    /* First calculate the normal QPR voltage-loop current reference. */
    for (axis = 0; axis < 2; ++axis)
    {
        ctrl->voltage_error_ab.dat[axis] =
            ctrl->voltage_ref_ab.dat[axis] - core->vab0.dat[axis];
        voltage_correction = ctl_step_qpr_controller(
            &ctrl->voltage_qpr[axis], ctrl->voltage_error_ab.dat[axis]);
        ctl_offgrid_limit_qr_state(
            &ctrl->voltage_qpr[axis].resonant_part,
            ctrl->current_limit_pu);
        voltage_correction = ctl_sat(
            voltage_correction,
            ctrl->current_limit_pu,
            -ctrl->current_limit_pu);

        ctrl->inductor_current_ref_ab.dat[axis] =
            core->iab0.dat[axis] +
            ctrl->capacitor_current_ref_ab.dat[axis] +
            voltage_correction;
    }

    if (!command_slew_complete)
    {
        /* Do not learn startup or voltage-command ramp transients. */
        for (axis = 0; axis < 2; ++axis)
        {
            servo.error_sum_ab[axis] = 0.0f;
            servo.error_mean_ab[axis] = 0.0f;
            servo.current_correction_ab[axis] = 0.0f;
        }
        servo.sample_count = 0;
    }
    else
    {
        servo.error_sum_ab[phase_alpha] +=
            ctrl2float(ctrl->voltage_error_ab.dat[phase_alpha]);
        servo.error_sum_ab[phase_beta] +=
            ctrl2float(ctrl->voltage_error_ab.dat[phase_beta]);
        ++servo.sample_count;

        if (cycle_wrapped && servo.sample_count > 0)
        {
            integration_step = GFL_LEVEL6_DC_SERVO_KI_PER_S /
                               ctrl->frequency_active_hz;

            for (axis = 0; axis < 2; ++axis)
            {
                servo.error_mean_ab[axis] =
                    servo.error_sum_ab[axis] /
                    (parameter_gt)servo.sample_count;

                if (modulation_has_margin &&
                    ctl_level6_dc_servo_abs(servo.error_mean_ab[axis]) >
                        GFL_LEVEL6_DC_SERVO_DEADBAND_PU)
                {
                    servo.current_correction_ab[axis] +=
                        integration_step * servo.error_mean_ab[axis];
                    servo.current_correction_ab[axis] =
                        ctl_level6_dc_servo_clamp(
                            servo.current_correction_ab[axis],
                            GFL_LEVEL6_DC_SERVO_LIMIT_PU);
                }

                servo.error_sum_ab[axis] = 0.0f;
            }
            servo.sample_count = 0;
        }
    }

    /*
     * Add the 0-Hz correction to the voltage-loop current command.  The
     * existing circular current-reference limit and current QPR controller
     * remain responsible for executing and damping this correction.
     */
    ctrl->inductor_current_ref_ab.dat[phase_alpha] +=
        float2ctrl(servo.current_correction_ab[phase_alpha]);
    ctrl->inductor_current_ref_ab.dat[phase_beta] +=
        float2ctrl(servo.current_correction_ab[phase_beta]);
    ctl_offgrid_limit_vector(
        &ctrl->inductor_current_ref_ab,
        ctrl->current_limit_pu);

    /* Current loop, voltage feed-forward, active damping and Vdc scaling. */
    for (axis = 0; axis < 2; ++axis)
    {
        current_correction = ctl_step_qpr_controller(
            &ctrl->current_qpr[axis],
            ctrl->inductor_current_ref_ab.dat[axis] -
                ctrl->inductor_current_est_ab.dat[axis]);
        ctl_offgrid_limit_qr_state(
            &ctrl->current_qpr[axis].resonant_part,
            ctrl->modulation_limit_pu);
        current_correction = ctl_sat(
            current_correction,
            ctrl->modulation_limit_pu,
            -ctrl->modulation_limit_pu);

        ctrl->modulation_ab.dat[axis] = ctl_mul(
            dc_bus_gain_ctrl,
            ctl_mul(ctrl->voltage_feedforward_gain,
                    core->vab0.dat[axis]) +
                current_correction -
                ctl_mul(ctrl->active_damping_gain,
                        ctrl->capacitor_current_est_ab.dat[axis]));
    }
    ctl_offgrid_limit_vector(
        &ctrl->modulation_ab,
        ctrl->modulation_limit_pu);
#else
    ctl_step_offgrid_voltage_ctrl(ctrl, core, enabled);
#endif
}

#endif // _FILE_PGS_INV_GFL_LEVEL6_DC_OFFSET_SERVO_H_
