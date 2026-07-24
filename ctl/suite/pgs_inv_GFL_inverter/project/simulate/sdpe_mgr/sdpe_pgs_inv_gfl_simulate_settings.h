/**
 * @file sdpe_pgs_inv_gfl_simulate_settings.h
 * @brief SDPE project bindings for PGS GFL Inverter Simulation.
 * @note Host simulation timing and sensing settings for the common GFL controller.
 */

#ifndef _PROJECT_SDPE_PGS_INV_GFL_SIMULATE_SETTINGS_H_
#define _PROJECT_SDPE_PGS_INV_GFL_SIMULATE_SETTINGS_H_

#include <ctl/hardware_preset/grid_lc_filter/gmp_harmonia_3ph_lc_filter.h>
#include <ctl/hardware_preset/inverter_3ph/gmp_helios_3phganinv_lv.h>

#ifdef __cplusplus
extern "C"
{
#endif

// User project prefix code
#include <sdpe_pgs_inv_gfl_common_settings.h>

//=================================================================================================
/**
 * @brief Project metadata.
 */

#define PGS_INV_GFL_SIM_SDPE_PROJECT_ID "pgs_inv_gfl_simulate"
#define PGS_INV_GFL_SIM_SDPE_PROJECT_SUITE "pgs_inv_GFL_inverter"
#define PGS_INV_GFL_SIM_SDPE_PROJECT_VERSION "1.0.0"
#define PGS_INV_GFL_SIM_SDPE_PROJECT_UPDATED_AT "2026-07-24"

//=================================================================================================
/**
 * @brief Commissioning.
 */

/**
 * @brief Incremental control level; level 5 is grid-connected P/Q control and level 6 is stand-alone line-voltage control for the resload model.
 *        Options: (1), (2), (3), (4), (5), (6)
 */
#define BUILD_LEVEL (6)

//=================================================================================================
/**
 * @brief Sampling.
 */

/**
 * @brief Number of directly sampled phase currents.
 *        Options: (2), (3)
 */
#define GFL_CURRENT_SAMPLE_PHASE_MODE (3)

/**
 * @brief Simulation supplies Uab and Ubc line-to-line voltage samples; mode 1 reconstructs stationary phase-voltage alpha/beta feedback.
 *        Options: (1), (2), (3)
 */
#define GFL_VOLTAGE_SAMPLE_PHASE_MODE (1)

//=================================================================================================
/**
 * @brief BUILD_LEVEL 6 RMS Amplitude Loop.
 */

/**
 * @brief Enable the slow RMS amplitude correction loop for BUILD_LEVEL 6.
 *        Options: (0), (1)
 */
#define GFL_LEVEL6_ENABLE_RMS_AMPLITUDE_LOOP (0)

//=================================================================================================
/**
 * @brief Requirement bindings.
 */

/**
 * @brief Simulation control step frequency.
 */
#define CONTROLLER_FREQUENCY (20e3)

/**
 * @brief Virtual PWM compare range.
 */
#define CTRL_PWM_CMP_MAX (3000)

/**
 * @brief Virtual PWM dead-band count.
 */
#define CTRL_PWM_DEADBAND_CMP (50)

/**
 * @brief Virtual ADC reference.
 */
#define CTRL_ADC_VOLTAGE_REF (3.3f)

/**
 * @brief DC-bus voltage base.
 */
#define CTRL_DCBUS_VOLTAGE (80.0f)

/**
 * @brief SVPWM phase-voltage base.
 */
#define CTRL_VOLTAGE_BASE (CTRL_DCBUS_VOLTAGE / 1.73205081f)

/**
 * @brief Phase-current base.
 */
#define CTRL_CURRENT_BASE (10.0f)

/**
 * @brief Harmonia inductance.
 */
#define GFL_GRID_FILTER_INDUCTANCE_H (HARMONIA_3PH_LC_FILTER_INDUCTANCE_H)

/**
 * @brief Harmonia capacitance.
 */
#define GFL_GRID_FILTER_CAPACITANCE_F (HARMONIA_3PH_LC_FILTER_CAPACITANCE_F)

/**
 * @brief DC voltage gain.
 */
#define CTRL_DC_VOLTAGE_SENSITIVITY (0.02738589f)

/**
 * @brief DC voltage bias.
 */
#define CTRL_DC_VOLTAGE_BIAS (0.0f)

/**
 * @brief Grid-voltage gain.
 */
#define CTRL_GRID_VOLTAGE_SENSITIVITY (0.0135135f)

/**
 * @brief Grid-voltage bias.
 */
#define CTRL_GRID_VOLTAGE_BIAS (1.65f)

/**
 * @brief Converter voltage gain.
 */
#define CTRL_INVERTER_VOLTAGE_SENSITIVITY (0.02738589f)

/**
 * @brief Converter voltage bias.
 */
#define CTRL_INVERTER_VOLTAGE_BIAS (0.0f)

/**
 * @brief DC current sensitivity.
 */
#define CTRL_DC_CURRENT_SENSITIVITY (0.02475f)

/**
 * @brief DC current bias.
 */
#define CTRL_DC_CURRENT_BIAS (1.65f)

/**
 * @brief Grid-current sensitivity.
 */
#define CTRL_GRID_CURRENT_SENSITIVITY (HARMONIA_3PH_LC_FILTER_PH_CURRENT_SENSITIVITY_MV_A * 0.001f)

/**
 * @brief Grid-current bias.
 */
#define CTRL_GRID_CURRENT_BIAS (HARMONIA_3PH_LC_FILTER_PH_CURRENT_ZERO_BIAS_V)

/**
 * @brief Converter current sensitivity.
 */
#define CTRL_INVERTER_CURRENT_SENSITIVITY (0.05f)

/**
 * @brief Converter current bias.
 */
#define CTRL_INVERTER_CURRENT_BIAS (1.65f)

/**
 * @brief Low-pass cutoff used by the Level 6 RMS voltage estimator.
 */
#define GFL_LEVEL6_RMS_FILTER_CUTOFF_HZ (10.0f)

/**
 * @brief Integral gain of the slow Level 6 RMS amplitude correction loop.
 */
#define GFL_LEVEL6_RMS_INTEGRAL_GAIN_PER_S (1.0f)

/**
 * @brief Minimum internal amplitude correction multiplier.
 */
#define GFL_LEVEL6_RMS_GAIN_MIN (0.95f)

/**
 * @brief Maximum internal amplitude correction multiplier.
 */
#define GFL_LEVEL6_RMS_GAIN_MAX (1.05f)

/**
 * @brief Delay after voltage soft start before enabling RMS amplitude correction.
 */
#define GFL_LEVEL6_RMS_SETTLE_TIME_S (0.30f)

/**
 * @brief Startup delay in milliseconds.
 */
#define CTRL_STARTUP_DELAY (50)

// User project tail code
#if (BUILD_LEVEL < 1) || (BUILD_LEVEL > 6)
#error BUILD_LEVEL_must_be_between_1_and_6
#endif

#ifdef __cplusplus
}
#endif

#endif // _PROJECT_SDPE_PGS_INV_GFL_SIMULATE_SETTINGS_H_
