/**
 * @file sdpe_pgs_inv_gfl_common_settings.h
 * @brief SDPE project bindings for PGS Grid-Following Inverter Common Settings.
 * @note Platform-independent control contract shared by all pgs_inv_GFL_inverter projects. Hardware timing, sensing, board resources and BUILD_LEVEL selection remain in each platform requirement.
 */

#ifndef _PROJECT_SDPE_PGS_INV_GFL_COMMON_SETTINGS_H_
#define _PROJECT_SDPE_PGS_INV_GFL_COMMON_SETTINGS_H_

#ifdef __cplusplus
extern "C"
{
#endif

// User project prefix code
/* Platform-independent GFL controller settings. */

//=================================================================================================
/**
 * @brief Project metadata.
 */

#define PGS_INV_GFL_COMMON_SDPE_PROJECT_ID "pgs_inv_gfl_common"
#define PGS_INV_GFL_COMMON_SDPE_PROJECT_SUITE "pgs_inv_GFL_inverter"
#define PGS_INV_GFL_COMMON_SDPE_PROJECT_VERSION "1.0.0"
#define PGS_INV_GFL_COMMON_SDPE_PROJECT_UPDATED_AT "2026-07-29"

//=================================================================================================
/**
 * @brief Control Algorithm.
 */

/**
 * @brief Enable the existing discrete PID anti-saturation path.
 */
#define _USE_DEBUG_DISCRETE_PID

/**
 * @brief Use DSOGI PLL instead of the default SRF PLL.
 */
// #define USING_DSOGI_PLL

/**
 * @brief Use the three-level NPC modulator instead of the two-level SPWM modulator.
 */
// #define USING_NPC_MODULATOR

//=================================================================================================
/**
 * @brief Runtime.
 */

/**
 * @brief Enable startup ADC offset calibration. Only enable while all calibrated power inputs are in a known zero state.
 */
// #define SPECIFY_ENABLE_ADC_CALIBRATE

/**
 * @brief Enable processor-in-the-loop input/output handling.
 */
// #define ENABLE_GMP_DL_PIL_SIM

/**
 * @brief Enable CiA402/GMP framework debug information.
 */
// #define GMP_CTL_FM_CONFIG_ENABLE_DEBUG_INFO

//=================================================================================================
/**
 * @brief Sampling.
 */

/**
 * @brief Capacitor-current source: direct measurement, current difference, or capacitor-voltage derivative.
 *        Options: (1), (2), (3)
 */
#define GFL_CAPACITOR_CURRENT_CALCULATE_MODE (3)

//=================================================================================================
/**
 * @brief Requirement bindings.
 */

/**
 * @brief Nominal grid phase-voltage magnitude in controller per unit.
 */
#define GFL_GRID_VOLTAGE_PU (0.33f)

/**
 * @brief P/Q outer-loop execution frequency in hertz.
 */
#define GFL_PQ_LOOP_FREQUENCY_HZ (1000.0f)

/**
 * @brief Current-ISR to P/Q-loop execution divider.
 */
#define GFL_PQ_LOOP_DIVIDER ((uint32_t)(CONTROLLER_FREQUENCY / GFL_PQ_LOOP_FREQUENCY_HZ))

/**
 * @brief Active-power loop proportional gain from P error PU to d-axis current PU.
 */
#define GFL_PQ_ACTIVE_KP (0.75f)

/**
 * @brief Active-power loop integral gain in inverse seconds.
 */
#define GFL_PQ_ACTIVE_KI (0.001f)

/**
 * @brief Reactive-power loop proportional gain from Q error PU to q-axis current PU.
 */
#define GFL_PQ_REACTIVE_KP (0.75f)

/**
 * @brief Reactive-power loop integral gain in inverse seconds.
 */
#define GFL_PQ_REACTIVE_KI (0.001f)

/**
 * @brief Circular magnitude limit applied to the d/q current reference produced by the P/Q loop.
 */
#define GFL_PQ_CURRENT_LIMIT_PU (1.0f)

/**
 * @brief BUILD_LEVEL 1 d-axis open-loop voltage command.
 */
#define GFL_OPEN_LOOP_VD_PU (0.3f)

/**
 * @brief BUILD_LEVEL 1 q-axis open-loop voltage command.
 */
#define GFL_OPEN_LOOP_VQ_PU (0.3f)

/**
 * @brief PLL lock-error threshold in controller per unit.
 */
#define CTRL_SPLL_EPSILON ((float2ctrl(0.005)))

/**
 * @brief Default active-power reference. Positive power exports energy to the grid.
 */
#define GFL_ACTIVE_POWER_REF_PU (0.1f)

/**
 * @brief Default reactive-power reference using Q = vq*id - vd*iq.
 */
#define GFL_REACTIVE_POWER_REF_PU (0.0f)

/**
 * @brief BUILD_LEVEL 2 d-axis current command.
 */
#define GFL_CURRENT_LEVEL2_ID_PU (0.2f)

/**
 * @brief BUILD_LEVEL 2 q-axis current command.
 */
#define GFL_CURRENT_LEVEL2_IQ_PU (0.2f)

/**
 * @brief BUILD_LEVEL 3 grid-connected d-axis current command.
 */
#define GFL_CURRENT_LEVEL3_ID_PU (0.1f)

/**
 * @brief BUILD_LEVEL 3 grid-connected q-axis current command.
 */
#define GFL_CURRENT_LEVEL3_IQ_PU (0.0f)

/**
 * @brief BUILD_LEVEL 4 d-axis current command.
 */
#define GFL_CURRENT_LEVEL4_ID_PU (0.6f)

/**
 * @brief BUILD_LEVEL 4 q-axis current command.
 */
#define GFL_CURRENT_LEVEL4_IQ_PU (0.6f)

/**
 * @brief BUILD_LEVEL 6/7 positive d-axis output-voltage reference in per unit.
 */
#define GFL_LEVEL6_VD_REF_PU (0.754247f)

/**
 * @brief BUILD_LEVEL 6/7 positive q-axis output-voltage reference in per unit.
 */
#define GFL_LEVEL6_VQ_REF_PU (0.0f)

/**
 * @brief BUILD_LEVEL 6/7 d-axis voltage-loop proportional gain.
 */
#define GFL_LEVEL6_VOLTAGE_D_KP (0.0029f)

/**
 * @brief BUILD_LEVEL 6/7 d-axis series-PI integral frequency in radians per second.
 */
#define GFL_LEVEL6_VOLTAGE_D_KI (25.132741f)

/**
 * @brief BUILD_LEVEL 6/7 q-axis voltage-loop proportional gain.
 */
#define GFL_LEVEL6_VOLTAGE_Q_KP (0.0029f)

/**
 * @brief BUILD_LEVEL 6/7 q-axis series-PI integral frequency in radians per second.
 */
#define GFL_LEVEL6_VOLTAGE_Q_KI (25.132741f)

/**
 * @brief BUILD_LEVEL 6/7 circular positive d/q current-reference limit in per unit.
 */
#define GFL_LEVEL6_CURRENT_LIMIT_PU (0.35f)

/**
 * @brief BUILD_LEVEL 6 positive voltage-feedback 2-omega notch quality factor.
 */
#define GFL_LEVEL6_POS_VOLTAGE_NOTCH_Q (5.0f)

/**
 * @brief BUILD_LEVEL 6 negative-sequence 2-omega notch quality factor.
 */
#define GFL_LEVEL6_NEG_NOTCH_Q (5.0f)

/**
 * @brief BUILD_LEVEL 6 negative-sequence current-loop proportional gain.
 */
#define GFL_LEVEL6_NEG_CURRENT_KP (0.2040f)

/**
 * @brief BUILD_LEVEL 6 negative-sequence current-loop series-PI integral frequency in radians per second.
 */
#define GFL_LEVEL6_NEG_CURRENT_KI (62.831853f)

/**
 * @brief BUILD_LEVEL 6 negative current-loop voltage-output limit in per unit.
 */
#define GFL_LEVEL6_NEG_CURRENT_LIMIT_PU (0.3f)

/**
 * @brief BUILD_LEVEL 6 negative-sequence voltage-loop proportional gain.
 */
#define GFL_LEVEL6_NEG_VOLTAGE_KP (0.0029f)

/**
 * @brief BUILD_LEVEL 6 negative-sequence voltage-loop series-PI integral frequency in radians per second.
 */
#define GFL_LEVEL6_NEG_VOLTAGE_KI (25.132741f)

/**
 * @brief BUILD_LEVEL 6 negative voltage-loop current-reference limit in per unit.
 */
#define GFL_LEVEL6_NEG_VOLTAGE_LIMIT_PU (0.2f)

/**
 * @brief BUILD_LEVEL 7 DDSRF decoupling low-pass cutoff frequency in hertz.
 */
#define GFL_LEVEL7_DDSRF_FILTER_FC_HZ (42.426407f)

/**
 * @brief BUILD_LEVEL 7 negative current-loop Kp; defaults to the Level 6 value.
 */
#define GFL_LEVEL7_NEG_CURRENT_KP (GFL_LEVEL6_NEG_CURRENT_KP)

/**
 * @brief BUILD_LEVEL 7 negative current-loop Ki; defaults to the Level 6 value.
 */
#define GFL_LEVEL7_NEG_CURRENT_KI (GFL_LEVEL6_NEG_CURRENT_KI)

/**
 * @brief BUILD_LEVEL 7 negative current-loop output limit; defaults to Level 6.
 */
#define GFL_LEVEL7_NEG_CURRENT_LIMIT_PU (GFL_LEVEL6_NEG_CURRENT_LIMIT_PU)

/**
 * @brief BUILD_LEVEL 7 negative voltage-loop Kp; defaults to the Level 6 value.
 */
#define GFL_LEVEL7_NEG_VOLTAGE_KP (GFL_LEVEL6_NEG_VOLTAGE_KP)

/**
 * @brief BUILD_LEVEL 7 negative voltage-loop Ki; defaults to the Level 6 value.
 */
#define GFL_LEVEL7_NEG_VOLTAGE_KI (GFL_LEVEL6_NEG_VOLTAGE_KI)

/**
 * @brief BUILD_LEVEL 7 negative voltage-loop output limit; defaults to Level 6.
 */
#define GFL_LEVEL7_NEG_VOLTAGE_LIMIT_PU (GFL_LEVEL6_NEG_VOLTAGE_LIMIT_PU)

/**
 * @brief ADC offset calibrator filter cutoff frequency.
 */
#define GFL_ADC_CALIBRATOR_FC_HZ (20.0f)

/**
 * @brief ADC offset calibrator second-order filter quality factor.
 */
#define GFL_ADC_CALIBRATOR_Q (0.707f)

/**
 * @brief Minimum CiA402 delay before Operation Enabled.
 */
#define GFL_CIA402_OPERATION_ENABLE_DELAY_MS (100)

/**
 * @brief Nominal grid frequency in hertz.
 */
#define GFL_GRID_FREQUENCY_HZ (60.0f)

/**
 * @brief BUILD_LEVEL 2 phase-current ADC low-pass cutoff frequency in hertz.
 */
#define GFL_LEVEL2_CURRENT_ADC_FILTER_FC_HZ (2000.0f)

// User project tail code
/* Accept the historical PIL spelling while new projects use the canonical switch. */
#if defined ENBALE_GMP_DL_PIL_SIM && !defined ENABLE_GMP_DL_PIL_SIM
#define ENABLE_GMP_DL_PIL_SIM
#endif

#ifdef __cplusplus
}
#endif

#endif // _PROJECT_SDPE_PGS_INV_GFL_COMMON_SETTINGS_H_
