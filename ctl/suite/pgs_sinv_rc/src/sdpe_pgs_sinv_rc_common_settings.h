/**
 * @file sdpe_pgs_sinv_rc_common_settings.h
 * @brief SDPE project bindings for PGS Single-Phase Inverter Common Control.
 * @note Platform-independent control contract shared by all pgs_sinv_rc projects.
 */

#ifndef _PROJECT_SDPE_PGS_SINV_RC_COMMON_SETTINGS_H_
#define _PROJECT_SDPE_PGS_SINV_RC_COMMON_SETTINGS_H_

#ifdef __cplusplus
extern "C"
{
#endif

// User project prefix code
// SDPE extension point: add after_extern_open code in the Project Requirement Code page if needed.

//=================================================================================================
/**
 * @brief Project metadata.
 */

#define PGS_SINV_RC_COMMON_SDPE_PROJECT_ID "pgs_sinv_rc_common"
#define PGS_SINV_RC_COMMON_SDPE_PROJECT_SUITE "pgs_sinv_rc"
#define PGS_SINV_RC_COMMON_SDPE_PROJECT_VERSION "1.0.0"
#define PGS_SINV_RC_COMMON_SDPE_PROJECT_UPDATED_AT "2026-07-23"

//=================================================================================================
/**
 * @brief Control Features.
 */

/**
 * @brief Enable delayed insertion of the frequency-adaptive repetitive controller.
 */
#define SINV_ENABLE_REPETITIVE_CONTROL

/**
 * @brief Enable grid-voltage feedforward for closed-current-loop build levels.
 */
#define SINV_ENABLE_GRID_VOLTAGE_FEEDFORWARD

//=================================================================================================
/**
 * @brief Runtime.
 */

/**
 * @brief Allow ENABLE_OPERATION to advance through the complete CiA402 startup sequence.
 */
#define CIA402_CONFIG_ENABLE_SEQUENCE_SWITCH

//=================================================================================================
/**
 * @brief Requirement bindings.
 */

/**
 * @brief Minimum voltage magnitude used by the P/Q reference generator.
 */
#define CTRL_GRID_VMIN_PU (0.1f)

/**
 * @brief SOGI PLL proportional gain.
 */
#define CTRL_PLL_KP (10.0f)

/**
 * @brief SOGI PLL integral time constant in seconds.
 */
#define CTRL_PLL_TI (0.02f)

/**
 * @brief PLL error-filter cutoff frequency in Hz.
 */
#define CTRL_PLL_LPF_FC (20.0f)

/**
 * @brief PLL frequency-error lock threshold in PU.
 */
#define CTRL_SPLL_EPSILON (0.005f)

/**
 * @brief Power measurement low-pass cutoff frequency in Hz.
 */
#define CTRL_PQ_LPF_FC (200.0f)

/**
 * @brief Peak current command limit in PU.
 */
#define CTRL_CURRENT_LIMIT_PU (0.9f)

/**
 * @brief Active-power command slew limit in PU/s.
 */
#define CTRL_P_SLEW_PU_S (5.0f)

/**
 * @brief Reactive-power command slew limit in PU/s.
 */
#define CTRL_Q_SLEW_PU_S (5.0f)

/**
 * @brief Current deadband used by PWM dead-time compensation.
 */
#define CTRL_CURRENT_DB_PU (0.01f)

/**
 * @brief QPR current-loop crossover target in Hz.
 */
#define SINV_CURRENT_LOOP_BANDWIDTH_HZ (600.0f)

/**
 * @brief Minimum fundamental tracked by FDRC in Hz.
 */
#define CTRL_FDRC_MIN_FREQ (45.0f)

/**
 * @brief Settling time before repetitive control starts learning.
 */
#define SINV_FDRC_ENABLE_DELAY_MS (300)

/**
 * @brief Repetitive-control learning gain.
 */
#define SINV_FDRC_LEARNING_GAIN (0.10f)

/**
 * @brief FDRC robustness-filter cutoff frequency.
 */
#define SINV_FDRC_Q_FILTER_HZ (1000.0f)

/**
 * @brief Plant-delay compensation in controller samples.
 */
#define SINV_FDRC_LEAD_STEPS (3.0f)

/**
 * @brief Current-error threshold above which RC learning is frozen.
 */
#define SINV_FDRC_FREEZE_ERROR_PU (0.05f)

/**
 * @brief BUILD_LEVEL 1 sinusoidal H-bridge voltage amplitude.
 */
#define SINV_LEVEL1_VOLTAGE_REF_PU (0.35f)

/**
 * @brief Active-power outer-loop proportional gain.
 */
#define SINV_POWER_LOOP_KP (0.6f)

/**
 * @brief Active-power outer-loop integral gain per second.
 */
#define SINV_POWER_LOOP_KI (8.0f)

/**
 * @brief DC-bus outer-loop proportional gain.
 */
#define SINV_DC_BUS_LOOP_KP (0.8f)

/**
 * @brief DC-bus outer-loop integral gain per second.
 */
#define SINV_DC_BUS_LOOP_KI (12.0f)

/**
 * @brief Symmetric outer-loop active-power command limit.
 */
#define SINV_OUTER_LOOP_POWER_LIMIT_PU (0.65f)

/**
 * @brief Power and DC-bus outer-loop execution frequency.
 */
#define SINV_OUTER_LOOP_FREQUENCY_HZ (1000.0f)

/**
 * @brief Buck voltage-loop execution frequency.
 */
#define SINV_BUCK_VOLTAGE_LOOP_FREQUENCY_HZ (1000.0f)

/**
 * @brief Buck voltage-loop proportional gain.
 */
#define SINV_BUCK_VOLTAGE_LOOP_KP (0.35f)

/**
 * @brief Buck voltage-loop integral gain per second.
 */
#define SINV_BUCK_VOLTAGE_LOOP_KI (30.0f)

/**
 * @brief Buck current-loop proportional gain.
 */
#define SINV_BUCK_CURRENT_LOOP_KP (0.20f)

/**
 * @brief Buck current-loop integral gain per second.
 */
#define SINV_BUCK_CURRENT_LOOP_KI (500.0f)

/**
 * @brief BUILD_LEVEL 2 peak current command with a resistive load.
 */
#define SINV_LEVEL2_CURRENT_REF_PEAK_PU (0.20f)

/**
 * @brief BUILD_LEVEL 3 signed grid active-power command; positive exports power.
 */
#define SINV_LEVEL3_ACTIVE_POWER_REF_PU (0.10f)

/**
 * @brief BUILD_LEVEL 3 grid reactive-power command.
 */
#define SINV_LEVEL3_REACTIVE_POWER_REF_PU (0.0f)

/**
 * @brief BUILD_LEVEL 4 measured active-power closed-loop target.
 */
#define SINV_LEVEL4_ACTIVE_POWER_REF_PU (0.15f)

/**
 * @brief BUILD_LEVEL 5 physical DC bus voltage target.
 */
#define SINV_DC_BUS_REF_V (80.0f)

/**
 * @brief Buck output voltage target.
 */
#define SINV_BUCK_OUTPUT_REF_V (60.0f)

/**
 * @brief Buck inductor-current command limit in ampere.
 */
#define SINV_BUCK_CURRENT_LIMIT_A (5.0f)

/**
 * @brief Minimum operation-enabled transition delay.
 */
#define SINV_CIA402_OPERATION_ENABLE_DELAY_MS (100)

/**
 * @brief Minimum DC bus voltage before Buck soft-start is allowed.
 */
#define SINV_BUCK_START_VBUS_MIN_V (55.0f)

/**
 * @brief Delay after the Buck start condition is met before PWM compare ramps.
 */
#define SINV_BUCK_START_DELAY_MS (100)

/**
 * @brief Maximum Buck duty compare-command slew rate in PU per second.
 */
#define SINV_BUCK_DUTY_SLEW_PU_S (2.0f)

/**
 * @brief Buck duty-cycle lower clamp.
 */
#define SINV_BUCK_DUTY_MIN (0.0f)

/**
 * @brief Buck duty-cycle upper clamp.
 */
#define SINV_BUCK_DUTY_MAX (1.0f)

/**
 * @brief Maximum Buck current-loop duty correction around input-voltage feedforward.
 */
#define SINV_BUCK_DUTY_TRIM_LIMIT (0.03f)

/**
 * @brief Extra duty headroom above Buck voltage feedforward during startup and transients.
 */
#define SINV_BUCK_DUTY_FF_MARGIN (0.02f)

/**
 * @brief First-order low-pass coefficient for Buck input-voltage feedforward.
 */
#define SINV_BUCK_VIN_FF_LPF_ALPHA (0.006f)

/**
 * @brief Buck input-voltage feedforward blending gain; 0 disables source feedforward and 1 uses full Vref/Vin feedforward.
 */
#define SINV_BUCK_DUTY_FF_GAIN (0.35f)

/**
 * @brief Nominal grid frequency in Hz.
 */
#define CTRL_GRID_FREQUENCY (50.0f)

// User project tail code
// SDPE extension point: add before_footer code in the Project Requirement Code page if needed.

#ifdef __cplusplus
}
#endif

#endif // _PROJECT_SDPE_PGS_SINV_RC_COMMON_SETTINGS_H_
