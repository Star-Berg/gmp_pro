/**
 * @file level6_dc_offset_servo.h
 * @brief Slow cycle-average DC-voltage suppression loop for BUILD_LEVEL 6.
 *
 * The existing alpha/beta QPR loops regulate the commanded AC fundamental but
 * have only finite gain at 0 Hz.  This helper averages the alpha/beta voltage
 * error over one complete output period and integrates a small modulation
 * correction.  The correction is deliberately slow, bounded and disabled
 * during voltage-command slew or modulation saturation.
 */

#ifndef _FILE_PGS_INV_GFL_LEVEL6_DC_OFFSET_SERVO_H_
#define _FILE_PGS_INV_GFL_LEVEL6_DC_OFFSET_SERVO_H_

#ifndef GFL_LEVEL6_ENABLE_DC_OFFSET_SERVO
#define GFL_LEVEL6_ENABLE_DC_OFFSET_SERVO (1)
#endif

/* Modulation correction per voltage-error PU per second. */
#ifndef GFL_LEVEL6_DC_SERVO_KI_PER_S
#define GFL_LEVEL6_DC_SERVO_KI_PER_S (1.0f)
#endif

/* Ignore cycle-average voltage errors below this controller-PU magnitude. */
#ifndef GFL_LEVEL6_DC_SERVO_DEADBAND_PU
#define GFL_LEVEL6_DC_SERVO_DEADBAND_PU (0.001f)
#endif

/* Independent alpha/beta modulation-correction limit. */
#ifndef GFL_LEVEL6_DC_SERVO_LIMIT_PU
#define GFL_LEVEL6_DC_SERVO_LIMIT_PU (0.08f)
#endif

/* Freeze integration when the uncorrected modulation vector approaches limit. */
#ifndef GFL_LEVEL6_DC_SERVO_MODULATION_GUARD_PU
#define GFL_LEVEL6_DC_SERVO_MODULATION_GUARD_PU (0.93f)
#endif

typedef struct _tag_level6_dc_offset_servo
{
    parameter_gt error_sum_ab[2];
    parameter_gt error_mean_ab[2];
    parameter_gt modulation_correction_ab[2];
    uint32_t sample_count;
    ctrl_gt angle_last_pu;
    fast_gt initialized;
} level6_dc_offset_servo_t;

GMP_STATIC_INLINE parameter_gt ctl_level6_dc_servo_clamp(parameter_gt value, parameter_gt limit)
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

GMP_STATIC_INLINE void ctl_clear_level6_dc_offset_servo(level6_dc_offset_servo_t* servo)
{
    int axis;

    for (axis = 0; axis < 2; ++axis)
    {
        servo->error_sum_ab[axis] = 0.0f;
        servo->error_mean_ab[axis] = 0.0f;
        servo->modulation_correction_ab[axis] = 0.0f;
    }

    servo->sample_count = 0;
    servo->angle_last_pu = float2ctrl(0.0f);
    servo->initialized = 0;
}

/**
 * @brief Apply one automatic DC-offset-suppression step.
 *
 * Call this immediately after ctl_step_offgrid_voltage_ctrl() and before the
 * modulation command is copied to the SPWM object.
 */
GMP_STATIC_INLINE void ctl_step_level6_dc_offset_servo(offgrid_voltage_ctrl_t* ctrl,
                                                        fast_gt enabled)
{
#if GFL_LEVEL6_ENABLE_DC_OFFSET_SERVO
    /* One controller instance is used by the Level 6 dispatch path. */
    static level6_dc_offset_servo_t servo;
    parameter_gt modulation_alpha;
    parameter_gt modulation_beta;
    parameter_gt modulation_magnitude_sq;
    parameter_gt integration_step;
    fast_gt cycle_wrapped;
    fast_gt command_slew_complete;
    fast_gt modulation_has_margin;
    int axis;

    gmp_base_assert(ctrl);

    if (!enabled)
    {
        if (servo.initialized)
            ctl_clear_level6_dc_offset_servo(&servo);
        return;
    }

    if (!servo.initialized)
    {
        ctl_clear_level6_dc_offset_servo(&servo);
        servo.angle_last_pu = ctrl->angle_pu;
        servo.initialized = 1;
    }

    cycle_wrapped = ctrl->angle_pu < servo.angle_last_pu;
    servo.angle_last_pu = ctrl->angle_pu;

    command_slew_complete =
        ctl_level6_dc_servo_abs(ctrl->line_voltage_rms_active_v -
                                ctrl->line_voltage_rms_cmd_v) < 0.001f;

    modulation_alpha = ctrl2float(ctrl->modulation_ab.dat[phase_alpha]);
    modulation_beta = ctrl2float(ctrl->modulation_ab.dat[phase_beta]);
    modulation_magnitude_sq = modulation_alpha * modulation_alpha +
                              modulation_beta * modulation_beta;
    modulation_has_margin =
        modulation_magnitude_sq <
        (GFL_LEVEL6_DC_SERVO_MODULATION_GUARD_PU *
         GFL_LEVEL6_DC_SERVO_MODULATION_GUARD_PU);

    if (!command_slew_complete)
    {
        /* Do not learn startup or command-ramp transients. */
        servo.error_sum_ab[phase_alpha] = 0.0f;
        servo.error_sum_ab[phase_beta] = 0.0f;
        servo.error_mean_ab[phase_alpha] = 0.0f;
        servo.error_mean_ab[phase_beta] = 0.0f;
        servo.modulation_correction_ab[phase_alpha] = 0.0f;
        servo.modulation_correction_ab[phase_beta] = 0.0f;
        servo.sample_count = 0;
    }
    else
    {
        /*
         * voltage_error_ab = reference - feedback.  The sinusoidal reference
         * has zero cycle average, so this directly measures the negative of
         * the real output DC component in alpha/beta coordinates.
         */
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
                    servo.modulation_correction_ab[axis] +=
                        integration_step * servo.error_mean_ab[axis];
                    servo.modulation_correction_ab[axis] =
                        ctl_level6_dc_servo_clamp(
                            servo.modulation_correction_ab[axis],
                            GFL_LEVEL6_DC_SERVO_LIMIT_PU);
                }

                servo.error_sum_ab[axis] = 0.0f;
            }

            servo.sample_count = 0;
        }
    }

    /*
     * Add the slowly varying 0-Hz correction after the normal QPR controller.
     * A final circular limit preserves the existing SPWM modulation boundary.
     */
    ctrl->modulation_ab.dat[phase_alpha] +=
        float2ctrl(servo.modulation_correction_ab[phase_alpha]);
    ctrl->modulation_ab.dat[phase_beta] +=
        float2ctrl(servo.modulation_correction_ab[phase_beta]);
    ctl_offgrid_limit_vector(&ctrl->modulation_ab,
                             ctrl->modulation_limit_pu);
#else
    GMP_UNUSED_VAR(ctrl);
    GMP_UNUSED_VAR(enabled);
#endif
}

#endif // _FILE_PGS_INV_GFL_LEVEL6_DC_OFFSET_SERVO_H_
