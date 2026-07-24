/**
 * @file sinv_rectifier_trial_overrides.h
 * @brief Conservative commissioning overrides for the single-phase AFE rectifier trial branch.
 *
 * This file is intentionally included after the generated SDPE settings.  It keeps the
 * experiment isolated from the SDPE source files while the settings are being validated.
 * Once the values are confirmed in simulation and on the bench, move them into the
 * corresponding sdpe_requirement.json files and regenerate the bindings.
 */
#ifndef _SINV_RECTIFIER_TRIAL_OVERRIDES_H_
#define _SINV_RECTIFIER_TRIAL_OVERRIDES_H_

/* Rectifier-only validation: keep the downstream Buck stage off. */
#ifdef SINV_BUCK_START_VBUS_MIN_V
#undef SINV_BUCK_START_VBUS_MIN_V
#endif
#define SINV_BUCK_START_VBUS_MIN_V (1000.0f)

/*
 * Slow the DC-bus loop so it does not strongly follow the inherent 100 Hz
 * single-phase bus ripple during the first rectifier commissioning tests.
 */
#ifdef SINV_DC_BUS_LOOP_KP
#undef SINV_DC_BUS_LOOP_KP
#endif
#define SINV_DC_BUS_LOOP_KP (0.20f)

#ifdef SINV_DC_BUS_LOOP_KI
#undef SINV_DC_BUS_LOOP_KI
#endif
#define SINV_DC_BUS_LOOP_KI (2.0f)

/*
 * First-order feedback filter coefficient, updated at the 1 kHz outer-loop rate.
 * alpha=0.06 corresponds to approximately 10 Hz and attenuates the 100 Hz bus ripple.
 */
#define SINV_DC_BUS_FEEDBACK_LPF_ALPHA (0.06f)

#if !defined(SPECIFY_PC_ENVIRONMENT)
/*
 * Hardware-only safety limits for the TMCS1133B5A (150 mV/A, about +/-11 A
 * ADC span).  The trial current command is limited to 8 A peak and the fast
 * software trip is armed below sensor saturation.
 */
#ifdef CTRL_CURRENT_BASE
#undef CTRL_CURRENT_BASE
#endif
#define CTRL_CURRENT_BASE (10.0f)

#ifdef CTRL_RATED_CURRENT_RMS
#undef CTRL_RATED_CURRENT_RMS
#endif
#define CTRL_RATED_CURRENT_RMS (5.0f)

#ifdef CTRL_CURRENT_LIMIT_PU
#undef CTRL_CURRENT_LIMIT_PU
#endif
#define CTRL_CURRENT_LIMIT_PU (0.80f)

#ifdef CTRL_PROT_IAC_PEAK_MAX
#undef CTRL_PROT_IAC_PEAK_MAX
#endif
#define CTRL_PROT_IAC_PEAK_MAX (9.0f)

/*
 * A 24 Vrms source passively precharges the bridge to roughly 34 V peak.
 * The previous 48 V ready threshold could block CiA402 startup indefinitely.
 */
#ifdef CTRL_DCBUS_READY_MIN
#undef CTRL_DCBUS_READY_MIN
#endif
#define CTRL_DCBUS_READY_MIN (25.0f)
#endif /* !SPECIFY_PC_ENVIRONMENT */

#endif /* _SINV_RECTIFIER_TRIAL_OVERRIDES_H_ */
