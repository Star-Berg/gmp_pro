/**
 * @file ctl_level67_voltage.h
 * @brief BUILD_LEVEL 6/7 voltage loop and DDSRF sequence extraction.
 */

#ifndef _FILE_CTL_LEVEL67_VOLTAGE_H_
#define _FILE_CTL_LEVEL67_VOLTAGE_H_

#include <ctl/component/digital_power/inv/inv_neg_ctrl.h>

typedef struct _tag_gfl_ddsrf_channel_t
{
    ctl_vector2_t pos_raw;
    ctl_vector2_t neg_raw;
    ctl_vector2_t pos_decoupled;
    ctl_vector2_t neg_decoupled;
    ctl_vector2_t pos_dc;
    ctl_vector2_t neg_dc;
    ctl_filter_IIR1_t lpf_pos[2];
    ctl_filter_IIR1_t lpf_neg[2];
    fast_gt initialized;
} gfl_ddsrf_channel_t;

typedef struct _tag_gfl_level67_voltage_ctrl_t
{
    ctl_vector2_t vdq_set;
    ctl_vector2_t vdq_set_applied;
    ctl_vector2_t vdq_feedback;
    ctl_vector2_t idq_ref;
    ctl_pid_t pid_vdq[2];
    ctl_filter_IIR2_t notch_vdq[2];
    ctrl_gt soft_start_gain;
    ctrl_gt soft_start_step;
    ctrl_gt current_limit;
    ctrl_gt current_limit_sq;
    gfl_ddsrf_channel_t voltage_seq;
    gfl_ddsrf_channel_t current_seq;
} gfl_level67_voltage_ctrl_t;

GMP_STATIC_INLINE void ctl_clear_ddsrf_channel(gfl_ddsrf_channel_t* seq)
{
    ctl_vector2_clear(&seq->pos_raw);
    ctl_vector2_clear(&seq->neg_raw);
    ctl_vector2_clear(&seq->pos_decoupled);
    ctl_vector2_clear(&seq->neg_decoupled);
    ctl_vector2_clear(&seq->pos_dc);
    ctl_vector2_clear(&seq->neg_dc);
    ctl_clear_filter_iir1(&seq->lpf_pos[phase_d]);
    ctl_clear_filter_iir1(&seq->lpf_pos[phase_q]);
    ctl_clear_filter_iir1(&seq->lpf_neg[phase_d]);
    ctl_clear_filter_iir1(&seq->lpf_neg[phase_q]);
    seq->initialized = 0;
}

GMP_STATIC_INLINE void ctl_init_ddsrf_channel(gfl_ddsrf_channel_t* seq, parameter_gt fs, parameter_gt fc)
{
    ctl_init_filter_iir1_lpf(&seq->lpf_pos[phase_d], fs, fc);
    ctl_init_filter_iir1_lpf(&seq->lpf_pos[phase_q], fs, fc);
    ctl_init_filter_iir1_lpf(&seq->lpf_neg[phase_d], fs, fc);
    ctl_init_filter_iir1_lpf(&seq->lpf_neg[phase_q], fs, fc);
    ctl_clear_ddsrf_channel(seq);
}

GMP_STATIC_INLINE void ctl_set_iir1_state(ctl_filter_IIR1_t* filter, ctrl_gt value)
{
    filter->x1 = value;
    filter->y1 = value;
    filter->out = value;
}

GMP_STATIC_INLINE void ctl_step_ddsrf_channel(gfl_ddsrf_channel_t* seq, const ctl_vector2_t* ab,
                                               const ctl_vector2_t* phasor)
{
    ctrl_gt cos_theta = phasor->dat[phasor_cos];
    ctrl_gt sin_theta = phasor->dat[phasor_sin];
    ctrl_gt cos_2theta = ctl_mul(cos_theta, cos_theta) - ctl_mul(sin_theta, sin_theta);
    ctrl_gt sin_2theta = ctl_mul2(ctl_mul(sin_theta, cos_theta));

    ctl_ct_park2(ab, phasor, &seq->pos_raw);
    ctl_ct_park2_neg(ab, phasor, &seq->neg_raw);

    if (!seq->initialized)
    {
        ctl_vector2_copy(&seq->pos_dc, &seq->pos_raw);
        ctl_vector2_copy(&seq->pos_decoupled, &seq->pos_raw);
        ctl_vector2_clear(&seq->neg_decoupled);
        ctl_vector2_clear(&seq->neg_dc);
        ctl_set_iir1_state(&seq->lpf_pos[phase_d], seq->pos_dc.dat[phase_d]);
        ctl_set_iir1_state(&seq->lpf_pos[phase_q], seq->pos_dc.dat[phase_q]);
        ctl_set_iir1_state(&seq->lpf_neg[phase_d], 0);
        ctl_set_iir1_state(&seq->lpf_neg[phase_q], 0);
        seq->initialized = 1;
        return;
    }

    seq->pos_decoupled.dat[phase_d] = seq->pos_raw.dat[phase_d] -
                                      (ctl_mul(seq->neg_dc.dat[phase_d], cos_2theta) +
                                       ctl_mul(seq->neg_dc.dat[phase_q], sin_2theta));
    seq->pos_decoupled.dat[phase_q] = seq->pos_raw.dat[phase_q] -
                                      (ctl_mul(seq->neg_dc.dat[phase_q], cos_2theta) -
                                       ctl_mul(seq->neg_dc.dat[phase_d], sin_2theta));
    seq->neg_decoupled.dat[phase_d] = seq->neg_raw.dat[phase_d] -
                                      (ctl_mul(seq->pos_dc.dat[phase_d], cos_2theta) -
                                       ctl_mul(seq->pos_dc.dat[phase_q], sin_2theta));
    seq->neg_decoupled.dat[phase_q] = seq->neg_raw.dat[phase_q] -
                                      (ctl_mul(seq->pos_dc.dat[phase_q], cos_2theta) +
                                       ctl_mul(seq->pos_dc.dat[phase_d], sin_2theta));

    seq->pos_dc.dat[phase_d] =
        ctl_step_filter_iir1(&seq->lpf_pos[phase_d], seq->pos_decoupled.dat[phase_d]);
    seq->pos_dc.dat[phase_q] =
        ctl_step_filter_iir1(&seq->lpf_pos[phase_q], seq->pos_decoupled.dat[phase_q]);
    seq->neg_dc.dat[phase_d] =
        ctl_step_filter_iir1(&seq->lpf_neg[phase_d], seq->neg_decoupled.dat[phase_d]);
    seq->neg_dc.dat[phase_q] =
        ctl_step_filter_iir1(&seq->lpf_neg[phase_q], seq->neg_decoupled.dat[phase_q]);
}

GMP_STATIC_INLINE void ctl_clear_level67_voltage(gfl_level67_voltage_ctrl_t* ctrl)
{
    ctl_vector2_clear(&ctrl->vdq_set_applied);
    ctl_vector2_clear(&ctrl->vdq_feedback);
    ctl_vector2_clear(&ctrl->idq_ref);
    ctrl->soft_start_gain = 0;
    ctl_clear_pid(&ctrl->pid_vdq[phase_d]);
    ctl_clear_pid(&ctrl->pid_vdq[phase_q]);
    ctl_clear_biquad_filter(&ctrl->notch_vdq[phase_d]);
    ctl_clear_biquad_filter(&ctrl->notch_vdq[phase_q]);
    ctl_clear_ddsrf_channel(&ctrl->voltage_seq);
    ctl_clear_ddsrf_channel(&ctrl->current_seq);
}

GMP_STATIC_INLINE void ctl_init_level67_voltage(gfl_level67_voltage_ctrl_t* ctrl, parameter_gt fs,
                                                 parameter_gt grid_frequency, parameter_gt vd_kp,
                                                 parameter_gt vd_ki, parameter_gt vq_kp, parameter_gt vq_ki,
                                                 parameter_gt current_limit, parameter_gt notch_q,
                                                 parameter_gt ddsrf_fc, parameter_gt soft_start_time_ms)
{
    ctl_init_pid(&ctrl->pid_vdq[phase_d], vd_kp, vd_ki, 0, fs);
    ctl_init_pid(&ctrl->pid_vdq[phase_q], vq_kp, vq_ki, 0, fs);
    ctl_set_pid_limit(&ctrl->pid_vdq[phase_d], current_limit, -current_limit);
    ctl_set_pid_limit(&ctrl->pid_vdq[phase_q], current_limit, -current_limit);
    ctl_set_pid_int_limit(&ctrl->pid_vdq[phase_d], current_limit, -current_limit);
    ctl_set_pid_int_limit(&ctrl->pid_vdq[phase_q], current_limit, -current_limit);
    ctl_init_biquad_notch(&ctrl->notch_vdq[phase_d], fs, 2.0f * grid_frequency, notch_q);
    ctl_init_biquad_notch(&ctrl->notch_vdq[phase_q], fs, 2.0f * grid_frequency, notch_q);
    ctl_init_ddsrf_channel(&ctrl->voltage_seq, fs, ddsrf_fc);
    ctl_init_ddsrf_channel(&ctrl->current_seq, fs, ddsrf_fc);
    ctrl->current_limit = float2ctrl(current_limit);
    ctrl->current_limit_sq = ctl_mul(ctrl->current_limit, ctrl->current_limit);
    ctl_clear_level67_voltage(ctrl);

    if (soft_start_time_ms > 0)
        ctrl->soft_start_step = float2ctrl(1000.0f / (fs * soft_start_time_ms));
    else
        ctrl->soft_start_step = float2ctrl(1.0f);
}

GMP_STATIC_INLINE void ctl_step_level67_soft_start(gfl_level67_voltage_ctrl_t* ctrl)
{
    ctrl->soft_start_gain += ctrl->soft_start_step;
    if (ctrl->soft_start_gain > float2ctrl(1.0f))
        ctrl->soft_start_gain = float2ctrl(1.0f);

    ctrl->vdq_set_applied.dat[phase_d] = ctl_mul(ctrl->vdq_set.dat[phase_d], ctrl->soft_start_gain);
    ctrl->vdq_set_applied.dat[phase_q] = ctl_mul(ctrl->vdq_set.dat[phase_q], ctrl->soft_start_gain);
}

GMP_STATIC_INLINE void ctl_step_level67_voltage_pi(gfl_level67_voltage_ctrl_t* ctrl)
{
    ctl_pid_t* pid_d = &ctrl->pid_vdq[phase_d];
    ctl_pid_t* pid_q = &ctrl->pid_vdq[phase_q];

    ctl_step_level67_soft_start(ctrl);
    ctl_step_pid_ser(pid_d, ctrl->vdq_set_applied.dat[phase_d] - ctrl->vdq_feedback.dat[phase_d]);
    ctl_step_pid_ser(pid_q, ctrl->vdq_set_applied.dat[phase_q] - ctrl->vdq_feedback.dat[phase_q]);

    // Use the unsaturated PI terms so the vector limiter preserves the d/q direction.
    ctrl_gt id_ref = pid_d->p_term + pid_d->i_term + pid_d->d_term;
    ctrl_gt iq_ref = pid_q->p_term + pid_q->i_term + pid_q->d_term;
    ctrl_gt magnitude_sq = ctl_mul(id_ref, id_ref) + ctl_mul(iq_ref, iq_ref);

    if (magnitude_sq > ctrl->current_limit_sq)
    {
        ctrl_gt scale = ctl_div(ctrl->current_limit, ctl_sqrt(magnitude_sq));
        id_ref = ctl_mul(id_ref, scale);
        iq_ref = ctl_mul(iq_ref, scale);
        ctl_pid_clamping_correction_using_real_output(pid_d, id_ref);
        ctl_pid_clamping_correction_using_real_output(pid_q, iq_ref);
    }

    pid_d->out = id_ref;
    pid_q->out = iq_ref;
    ctrl->idq_ref.dat[phase_d] = id_ref;
    ctrl->idq_ref.dat[phase_q] = iq_ref;
}

GMP_STATIC_INLINE void ctl_step_level6_voltage(gfl_level67_voltage_ctrl_t* ctrl, const ctl_vector2_t* vdq)
{
    ctrl->vdq_feedback.dat[phase_d] = ctl_step_biquad_filter(&ctrl->notch_vdq[phase_d], vdq->dat[phase_d]);
    ctrl->vdq_feedback.dat[phase_q] = ctl_step_biquad_filter(&ctrl->notch_vdq[phase_q], vdq->dat[phase_q]);
    ctl_step_level67_voltage_pi(ctrl);
}

GMP_STATIC_INLINE void ctl_step_level7_voltage(gfl_level67_voltage_ctrl_t* ctrl, const ctl_vector2_t* vab,
                                                const ctl_vector2_t* iab, const ctl_vector2_t* phasor)
{
    ctl_step_ddsrf_channel(&ctrl->voltage_seq, vab, phasor);
    ctl_step_ddsrf_channel(&ctrl->current_seq, iab, phasor);
    ctl_vector2_copy(&ctrl->vdq_feedback, &ctrl->voltage_seq.pos_dc);
    ctl_step_level67_voltage_pi(ctrl);
}

GMP_STATIC_INLINE void ctl_step_level7_positive_current(gfl_inv_ctrl_t* gfl, const ctl_vector2_t* idq_feedback,
                                                        const ctl_vector2_t* vdq_feedforward)
{
    ctl_pid_t* pid_d = &gfl->pid_idq[phase_d];
    ctl_pid_t* pid_q = &gfl->pid_idq[phase_q];

    ctl_vector2_copy(&gfl->idq, idq_feedback);

    // The inner current controller regulates the voltage across the filter
    // inductor. Feed the measured output voltage forward so that the PI only
    // has to generate the inductor voltage drop instead of the full
    // fundamental output voltage.
    ctl_vector2_copy(&gfl->vdq_ff_external, vdq_feedforward);
    gfl->vdq_out.dat[phase_d] =
        ctl_step_pid_ser(pid_d, gfl->idq_set.dat[phase_d] - gfl->idq.dat[phase_d]) +
        gfl->vdq_ff_external.dat[phase_d];
    gfl->vdq_out.dat[phase_q] =
        ctl_step_pid_ser(pid_q, gfl->idq_set.dat[phase_q] - gfl->idq.dat[phase_q]) +
        gfl->vdq_ff_external.dat[phase_q];

    if (gfl->flag_enable_decouple)
    {
        gfl->vdq_ff_decouple.dat[phase_d] = ctl_mul(gfl->coef_ff_decouple, gfl->idq.dat[phase_q]);
        gfl->vdq_ff_decouple.dat[phase_q] = -ctl_mul(gfl->coef_ff_decouple, gfl->idq.dat[phase_d]);
        gfl->vdq_out.dat[phase_d] += gfl->vdq_ff_decouple.dat[phase_d];
        gfl->vdq_out.dat[phase_q] += gfl->vdq_ff_decouple.dat[phase_q];
    }
    else
    {
        ctl_vector2_clear(&gfl->vdq_ff_decouple);
    }

    // Keep the positive-sequence voltage command inside the SVPWM circle.
    // If limiting occurs, back-calculate the PI contribution after removing
    // feed-forward and decoupling to prevent integrator windup.
    ctrl_gt magnitude_sq = ctl_mul(gfl->vdq_out.dat[phase_d], gfl->vdq_out.dat[phase_d]) +
                           ctl_mul(gfl->vdq_out.dat[phase_q], gfl->vdq_out.dat[phase_q]);
    if (magnitude_sq > float2ctrl(1.0f))
    {
        ctrl_gt scale = ctl_div(float2ctrl(1.0f), ctl_sqrt(magnitude_sq));
        gfl->vdq_out.dat[phase_d] = ctl_mul(gfl->vdq_out.dat[phase_d], scale);
        gfl->vdq_out.dat[phase_q] = ctl_mul(gfl->vdq_out.dat[phase_q], scale);

        ctrl_gt vd_pid_real =
            gfl->vdq_out.dat[phase_d] - gfl->vdq_ff_external.dat[phase_d] - gfl->vdq_ff_decouple.dat[phase_d];
        ctrl_gt vq_pid_real =
            gfl->vdq_out.dat[phase_q] - gfl->vdq_ff_external.dat[phase_q] - gfl->vdq_ff_decouple.dat[phase_q];
        ctl_pid_clamping_correction_using_real_output(pid_d, vd_pid_real);
        ctl_pid_clamping_correction_using_real_output(pid_q, vq_pid_real);
        pid_d->out = vd_pid_real;
        pid_q->out = vq_pid_real;
    }

    ctl_vector2_copy(&gfl->vdq_out_comp, &gfl->vdq_out);
    ctl_ct_ipark2(&gfl->vdq_out_comp, &gfl->phasor, &gfl->vab_pos);
    gfl->vab0_out.dat[phase_alpha] = gfl->vab_pos.dat[phase_alpha] + gfl->vab0_ff_external.dat[phase_alpha];
    gfl->vab0_out.dat[phase_beta] = gfl->vab_pos.dat[phase_beta] + gfl->vab0_ff_external.dat[phase_beta];
    gfl->vab0_out.dat[phase_0] = gfl->vab0_ff_external.dat[phase_0];
}

GMP_STATIC_INLINE void ctl_step_neg_inv_ctrl_dq(inv_neg_ctrl_t* neg, const ctl_vector2_t* idqn,
                                                 const ctl_vector2_t* vdqn)
{
    if (!neg->flag_enable_negative_current_ctrl)
    {
        ctl_vector2_clear(&neg->vab_out);
        return;
    }

    ctl_vector2_copy(&neg->idqn_raw, idqn);
    ctl_vector2_copy(&neg->idqn, idqn);
    ctl_vector2_copy(&neg->vdqn_raw, vdqn);
    ctl_vector2_copy(&neg->vdqn, vdqn);

    if (neg->flag_enable_negative_voltage_ctrl)
    {
        neg->idqn_ref_int.dat[phase_d] = ctl_step_pid_ser(
            &neg->pid_vdqn[phase_d], neg->vdqn_set.dat[phase_d] - neg->vdqn.dat[phase_d]);
        neg->idqn_ref_int.dat[phase_q] = ctl_step_pid_ser(
            &neg->pid_vdqn[phase_q], neg->vdqn_set.dat[phase_q] - neg->vdqn.dat[phase_q]);
    }
    else
    {
        ctl_vector2_copy(&neg->idqn_ref_int, &neg->idqn_set);
    }

    neg->vdqn_out.dat[phase_d] = ctl_step_pid_ser(
        &neg->pid_idqn[phase_d], neg->idqn_ref_int.dat[phase_d] - neg->idqn.dat[phase_d]);
    neg->vdqn_out.dat[phase_q] = ctl_step_pid_ser(
        &neg->pid_idqn[phase_q], neg->idqn_ref_int.dat[phase_q] - neg->idqn.dat[phase_q]);
    ctl_ct_ipark2_neg(&neg->vdqn_out, neg->phasor, &neg->vab_out);
}

#endif // _FILE_CTL_LEVEL67_VOLTAGE_H_
