/**
 * @file offgrid_voltage_ctrl.h
 * @brief BUILD_LEVEL 6 stationary-frame voltage and current controller.
 *
 * The controller is intentionally local to this suite.  It turns the existing
 * GFL demo into a stand-alone voltage source without changing the reusable GFL
 * component API.  The controlled voltage is the LC-capacitor/load voltage.
 *
 * Control chain:
 *
 *   calibrated balanced alpha/beta voltage reference
 *     -> alpha/beta QPR voltage loop
 *     -> load-current + capacitor-current feed-forward
 *     -> estimated inductor-current reference
 *     -> alpha/beta QPR current loop
 *     -> capacitor-voltage feed-forward + active damping
 *     -> rate-limited DC-bus feed-forward normalization
 *     -> SPWM modulation command
 *
 * Both stationary axes use the same resonant frequency, so positive- and
 * negative-sequence disturbances are regulated without a PLL.  This is useful
 * for the competition's unbalanced-load test.  Only alpha/beta quantities are
 * controlled; a three-leg, three-wire bridge cannot regulate zero sequence.
 */

#ifndef _FILE_PGS_INV_GFL_OFFGRID_VOLTAGE_CTRL_H_
#define _FILE_PGS_INV_GFL_OFFGRID_VOLTAGE_CTRL_H_

#include <ctl/component/digital_power/inv/gfl_core.h>
#include <ctl/component/intrinsic/discrete/proportional_resonant.h>

/* Fixed reference-amplitude calibration. */
#ifndef GFL_LEVEL6_VOLTAGE_REFERENCE_GAIN
#define GFL_LEVEL6_VOLTAGE_REFERENCE_GAIN (1.0f)
#endif

/* Experimental measured DC-bus feed-forward normalization. */
#ifndef GFL_LEVEL6_ENABLE_DCBUS_FEEDFORWARD
#define GFL_LEVEL6_ENABLE_DCBUS_FEEDFORWARD (1)
#endif

#ifndef GFL_LEVEL6_DCBUS_VALID_MIN_V
#define GFL_LEVEL6_DCBUS_VALID_MIN_V (50.0f)
#endif

#ifndef GFL_LEVEL6_DCBUS_VALID_MAX_V
#define GFL_LEVEL6_DCBUS_VALID_MAX_V (100.0f)
#endif

#ifndef GFL_LEVEL6_DCBUS_GAIN_MIN
#define GFL_LEVEL6_DCBUS_GAIN_MIN (0.75f)
#endif

#ifndef GFL_LEVEL6_DCBUS_GAIN_MAX
#define GFL_LEVEL6_DCBUS_GAIN_MAX (1.50f)
#endif

/* Maximum change of the feed-forward multiplier per second. */
#ifndef GFL_LEVEL6_DCBUS_GAIN_SLEW_PER_S
#define GFL_LEVEL6_DCBUS_GAIN_SLEW_PER_S (5.0f)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _tag_offgrid_voltage_ctrl_init
{
    parameter_gt fs;
    parameter_gt dc_bus_voltage;
    parameter_gt voltage_base;
    parameter_gt current_base;
    parameter_gt filter_inductance;
    parameter_gt filter_capacitance;

    parameter_gt voltage_loop_bw_hz;
    parameter_gt current_loop_bw_hz;
    parameter_gt voltage_qpr_kr;
    parameter_gt current_qpr_kr;
    parameter_gt qpr_bandwidth_hz;

    parameter_gt current_limit_pu;
    parameter_gt modulation_limit_pu;
    parameter_gt active_damping_resistance_ohm;
    parameter_gt voltage_slew_v_per_s;

    parameter_gt default_line_voltage_rms_v;
    parameter_gt default_frequency_hz;
} offgrid_voltage_ctrl_init_t;

typedef struct _tag_offgrid_voltage_ctrl
{
    /* Runtime commands.  Voltage is explicitly line-to-line RMS. */
    parameter_gt line_voltage_rms_cmd_v;
    parameter_gt line_voltage_rms_active_v;
    parameter_gt frequency_cmd_hz;
    parameter_gt frequency_active_hz;

    /* DC-bus feed-forward monitoring values, in physical volts and ratio. */
    parameter_gt dc_bus_voltage_nominal_v;
    parameter_gt dc_bus_voltage_meas_v;
    parameter_gt dc_bus_feedforward_target_gain;
    parameter_gt dc_bus_feedforward_gain;
    parameter_gt dc_bus_gain_slew_step;

    /* Reference angle is per-unit: 1.0 == 2*pi. */
    ctrl_gt angle_pu;
    ctl_vector2_t phasor;

    /* Public monitoring quantities, all controller per unit except commands. */
    ctl_vector2_t voltage_ref_ab;
    ctl_vector2_t voltage_error_ab;
    ctl_vector2_t capacitor_current_ref_ab;
    ctl_vector2_t capacitor_current_est_ab;
    ctl_vector2_t inductor_current_ref_ab;
    ctl_vector2_t inductor_current_est_ab;
    ctl_vector2_t modulation_ab;

    /* QPR loops for alpha and beta axes. */
    qpr_ctrl_t voltage_qpr[2];
    qpr_ctrl_t current_qpr[2];

    /* Previous capacitor voltage for C*dv/dt current estimation. */
    ctl_vector2_t voltage_ab_last;

    /* Normalized coefficients used by the ISR. */
    parameter_gt fs;
    parameter_gt voltage_base;
    parameter_gt current_base;
    parameter_gt filter_capacitance;
    parameter_gt voltage_qpr_kr;
    parameter_gt current_qpr_kr;
    parameter_gt qpr_bandwidth_hz;

    ctrl_gt angle_step_pu;
    ctrl_gt voltage_slew_step_v;
    ctrl_gt capacitor_derivative_gain;
    ctrl_gt capacitor_reference_gain;
    ctrl_gt voltage_feedforward_gain;
    ctrl_gt active_damping_gain;
    ctrl_gt current_limit_pu;
    ctrl_gt modulation_limit_pu;

    fast_gt flag_was_enabled;
} offgrid_voltage_ctrl_t;

GMP_STATIC_INLINE parameter_gt ctl_offgrid_clamp_parameter(parameter_gt value, parameter_gt minimum,
                                                            parameter_gt maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

GMP_STATIC_INLINE parameter_gt ctl_offgrid_quantize_parameter(parameter_gt value, parameter_gt minimum,
                                                               parameter_gt maximum, parameter_gt step)
{
    parameter_gt bounded = ctl_offgrid_clamp_parameter(value, minimum, maximum);
    uint32_t step_count = (uint32_t)(((bounded - minimum) / step) + 0.5f);
    return minimum + (parameter_gt)step_count * step;
}

GMP_STATIC_INLINE void ctl_offgrid_limit_qr_state(qr_ctrl_t* qr, ctrl_gt limit)
{
    qr->output = ctl_sat(qr->output, limit, -limit);
    qr->output_1 = ctl_sat(qr->output_1, limit, -limit);
    qr->output_2 = ctl_sat(qr->output_2, limit, -limit);
}

GMP_STATIC_INLINE void ctl_offgrid_limit_vector(ctl_vector2_t* vector, ctrl_gt limit)
{
    ctrl_gt magnitude_sq = ctl_mul(vector->dat[phase_alpha], vector->dat[phase_alpha]) +
                           ctl_mul(vector->dat[phase_beta], vector->dat[phase_beta]);
    ctrl_gt limit_sq = ctl_mul(limit, limit);

    if (magnitude_sq > limit_sq)
    {
        ctrl_gt scale = ctl_div(limit, ctl_sqrt(magnitude_sq));
        vector->dat[phase_alpha] = ctl_mul(vector->dat[phase_alpha], scale);
        vector->dat[phase_beta] = ctl_mul(vector->dat[phase_beta], scale);
    }
}

/** Update both voltage- and current-loop resonant coefficients without clearing state. */
GMP_STATIC_INLINE void ctl_update_offgrid_frequency(offgrid_voltage_ctrl_t* ctrl, parameter_gt frequency_hz)
{
    parameter_gt wr = CTL_PARAM_CONST_2PI * frequency_hz;
    parameter_gt wc = CTL_PARAM_CONST_2PI * ctrl->qpr_bandwidth_hz;
    parameter_gt k_tustin = 2.0f * ctrl->fs;
    int axis;

    ctrl->frequency_active_hz = frequency_hz;
    ctrl->angle_step_pu = float2ctrl(frequency_hz / ctrl->fs);
    ctrl->capacitor_reference_gain =
        float2ctrl(CTL_PARAM_CONST_2PI * frequency_hz * ctrl->filter_capacitance * ctrl->voltage_base /
                   ctrl->current_base);

    for (axis = 0; axis < 2; ++axis)
    {
        ctl_calc_qr_ctrl_coef(&ctrl->voltage_qpr[axis].resonant_part.coef, ctrl->voltage_qpr_kr, wc, wr,
                              k_tustin);
        ctl_calc_qr_ctrl_coef(&ctrl->current_qpr[axis].resonant_part.coef, ctrl->current_qpr_kr, wc, wr,
                              k_tustin);
    }
}

/** Set and quantize the requested line-to-line RMS voltage. */
GMP_STATIC_INLINE void ctl_set_offgrid_line_voltage_rms(offgrid_voltage_ctrl_t* ctrl, parameter_gt voltage_rms_v)
{
    ctrl->line_voltage_rms_cmd_v =
        ctl_offgrid_quantize_parameter(voltage_rms_v, GFL_LEVEL6_VOLTAGE_MIN_RMS_V,
                                       GFL_LEVEL6_VOLTAGE_MAX_RMS_V, GFL_LEVEL6_VOLTAGE_STEP_RMS_V);
}

/** Set and quantize output frequency while preserving the accumulated phase. */
GMP_STATIC_INLINE void ctl_set_offgrid_frequency_hz(offgrid_voltage_ctrl_t* ctrl, parameter_gt frequency_hz)
{
    parameter_gt quantized =
        ctl_offgrid_quantize_parameter(frequency_hz, GFL_LEVEL6_FREQUENCY_MIN_HZ,
                                       GFL_LEVEL6_FREQUENCY_MAX_HZ, GFL_LEVEL6_FREQUENCY_STEP_HZ);
    ctrl->frequency_cmd_hz = quantized;
}

GMP_STATIC_INLINE void ctl_clear_offgrid_voltage_ctrl(offgrid_voltage_ctrl_t* ctrl)
{
    int axis;

    for (axis = 0; axis < 2; ++axis)
    {
        ctl_clear_qpr_controller(&ctrl->voltage_qpr[axis]);
        ctl_clear_qpr_controller(&ctrl->current_qpr[axis]);
    }

    ctrl->line_voltage_rms_active_v = 0.0f;
    ctrl->dc_bus_voltage_meas_v = ctrl->dc_bus_voltage_nominal_v;
    ctrl->dc_bus_feedforward_target_gain = 1.0f;
    ctrl->dc_bus_feedforward_gain = 1.0f;
    ctrl->angle_pu = float2ctrl(0.0f);
    ctl_set_phasor_via_angle(ctrl->angle_pu, &ctrl->phasor);

    ctl_vector2_clear(&ctrl->voltage_ref_ab);
    ctl_vector2_clear(&ctrl->voltage_error_ab);
    ctl_vector2_clear(&ctrl->capacitor_current_ref_ab);
    ctl_vector2_clear(&ctrl->capacitor_current_est_ab);
    ctl_vector2_clear(&ctrl->inductor_current_ref_ab);
    ctl_vector2_clear(&ctrl->inductor_current_est_ab);
    ctl_vector2_clear(&ctrl->modulation_ab);
    ctl_vector2_clear(&ctrl->voltage_ab_last);

    ctrl->flag_was_enabled = 0;
}

GMP_STATIC_INLINE void ctl_init_offgrid_voltage_ctrl(offgrid_voltage_ctrl_t* ctrl,
                                                      const offgrid_voltage_ctrl_init_t* init)
{
    parameter_gt voltage_kp;
    parameter_gt current_kp;
    int axis;

    gmp_base_assert(ctrl);
    gmp_base_assert(init);
    gmp_base_assert(init->fs > 0.0f);
    gmp_base_assert(init->dc_bus_voltage > 0.0f);
    gmp_base_assert(init->voltage_base > 0.0f);
    gmp_base_assert(init->current_base > 0.0f);

    ctrl->fs = init->fs;
    ctrl->voltage_base = init->voltage_base;
    ctrl->current_base = init->current_base;
    ctrl->filter_capacitance = init->filter_capacitance;
    ctrl->voltage_qpr_kr = init->voltage_qpr_kr;
    ctrl->current_qpr_kr = init->current_qpr_kr;
    ctrl->qpr_bandwidth_hz = init->qpr_bandwidth_hz;
    ctrl->dc_bus_voltage_nominal_v = init->dc_bus_voltage;
    ctrl->dc_bus_gain_slew_step = GFL_LEVEL6_DCBUS_GAIN_SLEW_PER_S / init->fs;

    /*
     * Voltage loop: voltage-pu -> current-pu.
     * Current loop: current-pu -> SPWM modulation index at nominal DC bus.
     */
    voltage_kp = init->filter_capacitance * CTL_PARAM_CONST_2PI * init->voltage_loop_bw_hz *
                 init->voltage_base / init->current_base;
    current_kp = 2.0f * init->filter_inductance * CTL_PARAM_CONST_2PI * init->current_loop_bw_hz *
                 init->current_base / init->dc_bus_voltage;

    for (axis = 0; axis < 2; ++axis)
    {
        ctl_init_qpr_controller(&ctrl->voltage_qpr[axis], voltage_kp, init->voltage_qpr_kr,
                                init->default_frequency_hz, init->qpr_bandwidth_hz, init->fs);
        ctl_init_qpr_controller(&ctrl->current_qpr[axis], current_kp, init->current_qpr_kr,
                                init->default_frequency_hz, init->qpr_bandwidth_hz, init->fs);
    }

    ctrl->voltage_slew_step_v = float2ctrl(init->voltage_slew_v_per_s / init->fs);
    ctrl->capacitor_derivative_gain = float2ctrl(init->filter_capacitance * init->fs * init->voltage_base /
                                                 init->current_base);
    ctrl->voltage_feedforward_gain = float2ctrl(2.0f * init->voltage_base / init->dc_bus_voltage);
    ctrl->active_damping_gain =
        float2ctrl(2.0f * init->active_damping_resistance_ohm * init->current_base / init->dc_bus_voltage);
    ctrl->current_limit_pu = float2ctrl(init->current_limit_pu);
    ctrl->modulation_limit_pu = float2ctrl(init->modulation_limit_pu);

    ctrl->line_voltage_rms_cmd_v = init->default_line_voltage_rms_v;
    ctrl->frequency_cmd_hz = init->default_frequency_hz;
    ctrl->frequency_active_hz = init->default_frequency_hz;
    ctl_clear_offgrid_voltage_ctrl(ctrl);
    ctl_set_offgrid_line_voltage_rms(ctrl, init->default_line_voltage_rms_v);
    ctl_set_offgrid_frequency_hz(ctrl, init->default_frequency_hz);
    ctl_update_offgrid_frequency(ctrl, ctrl->frequency_cmd_hz);
}

/** Execute one 20 kHz control step using filtered feedback from the GFL core. */
GMP_STATIC_INLINE void ctl_step_offgrid_voltage_ctrl(offgrid_voltage_ctrl_t* ctrl, const gfl_inv_ctrl_t* core,
                                                      fast_gt enabled)
{
    ctrl_gt voltage_ref_peak_pu;
    ctrl_gt voltage_step;
    ctrl_gt voltage_correction;
    ctrl_gt current_correction;
    ctrl_gt delta_voltage;
    ctrl_gt dc_bus_gain_ctrl;
    parameter_gt quantized_command;
    parameter_gt dc_bus_gain_delta;
    int axis;

    gmp_base_assert(ctrl);
    gmp_base_assert(core);

    if (!enabled)
    {
        if (ctrl->flag_was_enabled)
            ctl_clear_offgrid_voltage_ctrl(ctrl);
        return;
    }

    if (!ctrl->flag_was_enabled)
    {
        ctrl->voltage_ab_last.dat[phase_alpha] = core->vab0.dat[phase_alpha];
        ctrl->voltage_ab_last.dat[phase_beta] = core->vab0.dat[phase_beta];
    }
    ctrl->flag_was_enabled = 1;

    /* Accept direct Datalink writes while retaining the public API limits. */
    quantized_command =
        ctl_offgrid_quantize_parameter(ctrl->line_voltage_rms_cmd_v, GFL_LEVEL6_VOLTAGE_MIN_RMS_V,
                                       GFL_LEVEL6_VOLTAGE_MAX_RMS_V, GFL_LEVEL6_VOLTAGE_STEP_RMS_V);
    ctrl->line_voltage_rms_cmd_v = quantized_command;

    quantized_command =
        ctl_offgrid_quantize_parameter(ctrl->frequency_cmd_hz, GFL_LEVEL6_FREQUENCY_MIN_HZ,
                                       GFL_LEVEL6_FREQUENCY_MAX_HZ, GFL_LEVEL6_FREQUENCY_STEP_HZ);
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

    /*
     * The GFL core filters the decoded DC-bus ADC value in per unit.  Convert
     * it back to volts and calculate nominal/measured feed-forward.  Invalid
     * measurements command a smooth return to unity rather than a jump.
     */
#if GFL_LEVEL6_ENABLE_DCBUS_FEEDFORWARD
    ctrl->dc_bus_voltage_meas_v =
        ctrl2float(core->filter_udc.out) * ctrl->voltage_base;

    if (ctrl->dc_bus_voltage_meas_v >= GFL_LEVEL6_DCBUS_VALID_MIN_V &&
        ctrl->dc_bus_voltage_meas_v <= GFL_LEVEL6_DCBUS_VALID_MAX_V)
    {
        ctrl->dc_bus_feedforward_target_gain =
            ctrl->dc_bus_voltage_nominal_v / ctrl->dc_bus_voltage_meas_v;
        ctrl->dc_bus_feedforward_target_gain =
            ctl_offgrid_clamp_parameter(ctrl->dc_bus_feedforward_target_gain,
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

    /*
     * Rate-limit the compensation multiplier so an 80 V -> 70 V bus step does
     * not instantly apply the full 14.3 percent modulation change to the LC
     * filter.  At the default 5 /s limit, the transition takes about 29 ms.
     */
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
    ctl_set_phasor_via_angle(ctrl->angle_pu, &ctrl->phasor);

    /* V_phase,peak = sqrt(2/3) * V_line,rms. */
    voltage_ref_peak_pu =
        float2ctrl(0.816496580927726f * ctrl->line_voltage_rms_active_v *
                   GFL_LEVEL6_VOLTAGE_REFERENCE_GAIN / ctrl->voltage_base);
    ctrl->voltage_ref_ab.dat[phase_alpha] = ctl_mul(voltage_ref_peak_pu, ctrl->phasor.dat[phasor_cos]);
    ctrl->voltage_ref_ab.dat[phase_beta] = ctl_mul(voltage_ref_peak_pu, ctrl->phasor.dat[phasor_sin]);

    /* Estimate capacitor current, then iL = iLoad + iC. */
    for (axis = 0; axis < 2; ++axis)
    {
        delta_voltage = core->vab0.dat[axis] - ctrl->voltage_ab_last.dat[axis];
        ctrl->capacitor_current_est_ab.dat[axis] = ctl_mul(ctrl->capacitor_derivative_gain, delta_voltage);
        ctrl->inductor_current_est_ab.dat[axis] =
            core->iab0.dat[axis] + ctrl->capacitor_current_est_ab.dat[axis];
        ctrl->voltage_ab_last.dat[axis] = core->vab0.dat[axis];
    }

    /* Analytic capacitor-current feed-forward for the sinusoidal reference. */
    ctrl->capacitor_current_ref_ab.dat[phase_alpha] =
        -ctl_mul(ctl_mul(ctrl->capacitor_reference_gain, voltage_ref_peak_pu), ctrl->phasor.dat[phasor_sin]);
    ctrl->capacitor_current_ref_ab.dat[phase_beta] =
        ctl_mul(ctl_mul(ctrl->capacitor_reference_gain, voltage_ref_peak_pu), ctrl->phasor.dat[phasor_cos]);

    /* QPR voltage loop adds the correction needed to keep both stationary axes sinusoidal. */
    for (axis = 0; axis < 2; ++axis)
    {
        ctrl->voltage_error_ab.dat[axis] = ctrl->voltage_ref_ab.dat[axis] - core->vab0.dat[axis];
        voltage_correction = ctl_step_qpr_controller(&ctrl->voltage_qpr[axis],
                                                     ctrl->voltage_error_ab.dat[axis]);
        ctl_offgrid_limit_qr_state(&ctrl->voltage_qpr[axis].resonant_part, ctrl->current_limit_pu);
        voltage_correction = ctl_sat(voltage_correction, ctrl->current_limit_pu, -ctrl->current_limit_pu);

        ctrl->inductor_current_ref_ab.dat[axis] =
            core->iab0.dat[axis] + ctrl->capacitor_current_ref_ab.dat[axis] + voltage_correction;
    }
    ctl_offgrid_limit_vector(&ctrl->inductor_current_ref_ab, ctrl->current_limit_pu);

    /*
     * Current-loop output is first calculated for nominal DC bus.  Multiplying
     * the complete modulation vector by the rate-limited Vdc ratio preserves
     * the applied bridge voltage without a hard feed-forward step.
     */
    for (axis = 0; axis < 2; ++axis)
    {
        current_correction = ctl_step_qpr_controller(
            &ctrl->current_qpr[axis],
            ctrl->inductor_current_ref_ab.dat[axis] - ctrl->inductor_current_est_ab.dat[axis]);
        ctl_offgrid_limit_qr_state(&ctrl->current_qpr[axis].resonant_part, ctrl->modulation_limit_pu);
        current_correction = ctl_sat(current_correction, ctrl->modulation_limit_pu, -ctrl->modulation_limit_pu);

        ctrl->modulation_ab.dat[axis] = ctl_mul(
            dc_bus_gain_ctrl,
            ctl_mul(ctrl->voltage_feedforward_gain, core->vab0.dat[axis]) +
                current_correction -
                ctl_mul(ctrl->active_damping_gain,
                        ctrl->capacitor_current_est_ab.dat[axis]));
    }
    ctl_offgrid_limit_vector(&ctrl->modulation_ab, ctrl->modulation_limit_pu);
}

#ifdef __cplusplus
}
#endif

#endif // _FILE_PGS_INV_GFL_OFFGRID_VOLTAGE_CTRL_H_
