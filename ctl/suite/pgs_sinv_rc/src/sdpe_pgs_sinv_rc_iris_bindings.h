/**
 * @file sdpe_pgs_sinv_rc_iris_bindings.h
 * @brief SDPE project bindings for PGS SINV RC F280039C IRIS Node.
 * @note Single-phase inverter project requirement prepared from xplt/ctrl_settings.h.
 *       This requirement introduces the GMP LVFB 150V 2-phase inverter board as the switching stage and the GMP Harmonia 3-phase LC filter as the grid filter. IRIS F280039C is included as the peripheral option provider.
 */

#ifndef _PROJECT_SDPE_PGS_SINV_RC_IRIS_BINDINGS_H_
#define _PROJECT_SDPE_PGS_SINV_RC_IRIS_BINDINGS_H_

#include <ctl/hardware_preset/grid_lc_filter/gmp_harmonia_3ph_lc_filter.h>
#include <ctl/hardware_preset/half_bridge/gmp_lvfb_150_2ph_v2.h>
#include <ctl/hardware_preset/mcu_board/iris_f280039c_node.h>

#ifdef __cplusplus
extern "C"
{
#endif

// User project prefix code
/* Original ctrl_settings.h includes are intentionally ignored during this SDPE migration trial. */

//=================================================================================================
/**
 * @brief Project metadata.
 */

#define SDPE_PROJECT_ID "pgs_sinv_rc_iris_node"
#define SDPE_PROJECT_SUITE "pgs_sinv_rc"
#define SDPE_PROJECT_VERSION "0.2.0"
#define SDPE_PROJECT_UPDATED_AT "2026-07-24"

//=================================================================================================
/**
 * @brief Controller Features.
 */

/**
 * @brief Enable Discrete PID controller anti-saturation algorithm.
 */
#define _USE_DEBUG_DISCRETE_PID

//=================================================================================================
/**
 * @brief Sensing and Calibration.
 */

/**
 * @brief Enable ADC calibration.
 */
#define SPECIFY_ENABLE_ADC_CALIBRATE

//=================================================================================================
/**
 * @brief Control Features.
 */

/**
 * @brief Allow ENABLE_OPERATION to advance through the complete CiA402 startup sequence.
 */
#define CIA402_CONFIG_ENABLE_SEQUENCE_SWITCH

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
 * @brief Diagnostics and Simulation.
 */

/**
 * @brief Enable PIL simulation function. This macro disables controller output.
 */
// #define ENABLE_GMP_DL_PIL_SIM

/**
 * @brief Enable CiA402 debug information.
 */
// #define GMP_CTL_FM_CONFIG_ENABLE_DEBUG_INFO

//=================================================================================================
/**
 * @brief Control Mode.
 */

/**
 * @brief Single-phase converter commissioning build level, aligned with the simulation controller branches.
 *        BUILD_LEVEL 1: open-loop H-bridge modulation for resistive-load validation.
 *        BUILD_LEVEL 2: sinusoidal current command for resistive-load current-loop validation.
 *        BUILD_LEVEL 3: grid current loop with signed P/Q command.
 *        BUILD_LEVEL 4: measured active-power outer loop feeding the grid current loop.
 *        BUILD_LEVEL 5: rectifier DC-bus voltage loop with PF-derived Q command.
 *        BUILD_LEVEL 6: bidirectional grid mode; direct signed P/Q by default, optional DC-bus loop generates signed P.
 *        Options: (1), (2), (3), (4), (5), (6)
 */
#define BUILD_LEVEL (5)

//=================================================================================================
/**
 * @brief PWM Modulator.
 */

/**
 * @brief Use negative PWM modulator logic.
 *        Options: (0), (1)
 */
#define PWM_MODULATOR_USING_NEGATIVE_LOGIC (0)

//=================================================================================================
/**
 * @brief PWM Channel Mapping.
 */

/**
 * @brief PWM base for inverter phase L.
 *        Options: IRIS_EPWM1_BASE, IRIS_EPWM2_BASE, IRIS_EPWM3_BASE, IRIS_EPWM4_BASE, IRIS_EPWM5_BASE, IRIS_EPWM6_BASE
 */
#define PHASE_L_BASE IRIS_EPWM3_BASE

/**
 * @brief PWM base for inverter phase N.
 *        Options: IRIS_EPWM1_BASE, IRIS_EPWM2_BASE, IRIS_EPWM3_BASE, IRIS_EPWM4_BASE, IRIS_EPWM5_BASE, IRIS_EPWM6_BASE
 */
#define PHASE_N_BASE IRIS_EPWM4_BASE

/**
 * @brief PWM base for the Buck half-bridge. This channel outputs buck_ctrl.pwm_cmp; leave the gate-drive wiring disconnected when the Buck stage is not installed.
 *        Options: IRIS_EPWM1_BASE, IRIS_EPWM2_BASE, IRIS_EPWM3_BASE, IRIS_EPWM4_BASE, IRIS_EPWM5_BASE, IRIS_EPWM6_BASE
 */
#define BUCK_PWM_BASE IRIS_EPWM5_BASE

//=================================================================================================
/**
 * @brief Gate Driver GPIO.
 */

/**
 * @brief Gate-driver enable GPIO.
 *        Options: IRIS_GPIO1, IRIS_GPIO2, IRIS_GPIO3, IRIS_GPIO4, IRIS_GPIO5, IRIS_GPIO6
 */
#define PWM_ENABLE_PORT IRIS_GPIO1

/**
 * @brief Gate-driver reset GPIO.
 *        Options: IRIS_GPIO1, IRIS_GPIO2, IRIS_GPIO3, IRIS_GPIO4, IRIS_GPIO5, IRIS_GPIO6
 */
#define PWM_RESET_PORT IRIS_GPIO3

//=================================================================================================
/**
 * @brief Status GPIO.
 */

/**
 * @brief System status LED.
 *        Options: IRIS_LED1, IRIS_LED2, LED_R, LED_G
 */
#define SYSTEM_LED IRIS_LED1

/**
 * @brief Controller status LED.
 *        Options: IRIS_LED1, IRIS_LED2, LED_R, LED_G
 */
#define CONTROLLER_LED IRIS_LED2

//=================================================================================================
/**
 * @brief AC Current Sensing.
 */

/**
 * @brief AC current ADC result register base.
 *        Options: ADC_CH1_RESULT_BASE, ADC_CH2_RESULT_BASE, ADC_CH3_RESULT_BASE, ADC_CH4_RESULT_BASE, ADC_CH5_RESULT_BASE, ADC_CH6_RESULT_BASE, ADC_CH7_RESULT_BASE, ADC_CH8_RESULT_BASE, ADC_CH9_RESULT_BASE, ADC_CH10_RESULT_BASE, ADC_CH11_RESULT_BASE, ADC_CH12_RESULT_BASE
 */
#define INV_IAC_RESULT_BASE ADC_CH1_RESULT_BASE

/**
 * @brief AC current ADC channel.
 *        Options: ADC_CH1, ADC_CH2, ADC_CH3, ADC_CH4, ADC_CH5, ADC_CH6, ADC_CH7, ADC_CH8, ADC_CH9, ADC_CH10, ADC_CH11, ADC_CH12
 */
#define INV_IAC ADC_CH1

//=================================================================================================
/**
 * @brief AC Voltage Sensing.
 */

/**
 * @brief AC voltage ADC result register base.
 *        Options: ADC_CH1_RESULT_BASE, ADC_CH2_RESULT_BASE, ADC_CH3_RESULT_BASE, ADC_CH4_RESULT_BASE, ADC_CH5_RESULT_BASE, ADC_CH6_RESULT_BASE, ADC_CH7_RESULT_BASE, ADC_CH8_RESULT_BASE, ADC_CH9_RESULT_BASE, ADC_CH10_RESULT_BASE, ADC_CH11_RESULT_BASE, ADC_CH12_RESULT_BASE
 */
#define INV_VAC_RESULT_BASE ADC_CH2_RESULT_BASE

/**
 * @brief AC voltage ADC channel.
 *        Options: ADC_CH1, ADC_CH2, ADC_CH3, ADC_CH4, ADC_CH5, ADC_CH6, ADC_CH7, ADC_CH8, ADC_CH9, ADC_CH10, ADC_CH11, ADC_CH12
 */
#define INV_VAC ADC_CH2

//=================================================================================================
/**
 * @brief DC Bus Sensing.
 */

/**
 * @brief DC bus voltage ADC result register base.
 *        Options: ADC_CH1_RESULT_BASE, ADC_CH2_RESULT_BASE, ADC_CH3_RESULT_BASE, ADC_CH4_RESULT_BASE, ADC_CH5_RESULT_BASE, ADC_CH6_RESULT_BASE, ADC_CH7_RESULT_BASE, ADC_CH8_RESULT_BASE, ADC_CH9_RESULT_BASE, ADC_CH10_RESULT_BASE, ADC_CH11_RESULT_BASE, ADC_CH12_RESULT_BASE
 */
#define INV_VBUS_RESULT_BASE ADC_CH3_RESULT_BASE

/**
 * @brief DC bus voltage ADC channel.
 *        Options: ADC_CH1, ADC_CH2, ADC_CH3, ADC_CH4, ADC_CH5, ADC_CH6, ADC_CH7, ADC_CH8, ADC_CH9, ADC_CH10, ADC_CH11, ADC_CH12
 */
#define INV_VBUS ADC_CH3

//=================================================================================================
/**
 * @brief Buck Current Sensing.
 */

/**
 * @brief Buck inductor-current ADC result register base, measured by the lower LVFB half-bridge current sensor.
 *        Options: ADC_CH1_RESULT_BASE, ADC_CH2_RESULT_BASE, ADC_CH3_RESULT_BASE, ADC_CH4_RESULT_BASE, ADC_CH5_RESULT_BASE, ADC_CH6_RESULT_BASE, ADC_CH7_RESULT_BASE, ADC_CH8_RESULT_BASE, ADC_CH9_RESULT_BASE, ADC_CH10_RESULT_BASE, ADC_CH11_RESULT_BASE, ADC_CH12_RESULT_BASE
 */
#define BUCK_IL_RESULT_BASE ADC_CH5_RESULT_BASE

/**
 * @brief Buck inductor-current ADC channel.
 *        Options: ADC_CH1, ADC_CH2, ADC_CH3, ADC_CH4, ADC_CH5, ADC_CH6, ADC_CH7, ADC_CH8, ADC_CH9, ADC_CH10, ADC_CH11, ADC_CH12
 */
#define BUCK_IL ADC_CH5

//=================================================================================================
/**
 * @brief Buck Output Voltage Sensing.
 */

/**
 * @brief Buck output-voltage ADC result register base. Use an external voltage measurement board; do not use the lower half-bridge DC bus sensor for this signal.
 *        Options: ADC_CH1_RESULT_BASE, ADC_CH2_RESULT_BASE, ADC_CH3_RESULT_BASE, ADC_CH4_RESULT_BASE, ADC_CH5_RESULT_BASE, ADC_CH6_RESULT_BASE, ADC_CH7_RESULT_BASE, ADC_CH8_RESULT_BASE, ADC_CH9_RESULT_BASE, ADC_CH10_RESULT_BASE, ADC_CH11_RESULT_BASE, ADC_CH12_RESULT_BASE
 */
#define BUCK_VOUT_RESULT_BASE ADC_CH4_RESULT_BASE

/**
 * @brief Buck output-voltage ADC channel.
 *        Options: ADC_CH1, ADC_CH2, ADC_CH3, ADC_CH4, ADC_CH5, ADC_CH6, ADC_CH7, ADC_CH8, ADC_CH9, ADC_CH10, ADC_CH11, ADC_CH12
 */
#define BUCK_VOUT ADC_CH4

//=================================================================================================
/**
 * @brief Requirement bindings.
 */

/**
 * @brief Controller ISR frequency.
 */
#define CONTROLLER_FREQUENCY (20e3)

/**
 * @brief PWM compare maximum.
 */
#define CTRL_PWM_CMP_MAX (3000 - 1)

/**
 * @brief PWM deadband compare value.
 */
#define CTRL_PWM_DEADBAND_CMP (50)

/**
 * @brief System tick divider derived from PWM period.
 */
#define DSP_C2000_DSP_TIME_DIV (120000 / CTRL_PWM_CMP_MAX / 2)

/**
 * @brief ADC voltage reference.
 */
#define CTRL_ADC_VOLTAGE_REF (3.3f)

/**
 * @brief Rated DC bus voltage.
 */
#define CTRL_DCBUS_VOLTAGE (60.0f)

/**
 * @brief Rated AC grid/load RMS voltage.
 */
#define CTRL_GRID_VOLTAGE_RMS (24.0f)

/**
 * @brief Rated AC output RMS current.
 */
#define CTRL_RATED_CURRENT_RMS (10.0f)

/**
 * @brief Voltage per-unit base, using peak value.
 */
#define CTRL_VOLTAGE_BASE (34.0f)

/**
 * @brief Current per-unit base, using peak value.
 */
#define CTRL_CURRENT_BASE (14.14f)

/**
 * @brief Total AC-side filter/grid inductance in H.
 */
#define CTRL_AC_INDUCTANCE (0.0015f)

/**
 * @brief Total AC-side series resistance in Ohm.
 */
#define CTRL_AC_RESISTANCE (0.1f)

/**
 * @brief DC bus voltage sensing gain from the LVFB inverter voltage sensor.
 */
#define CTRL_DC_VOLTAGE_SENSITIVITY GMP_LVFB_VOLTAGE_SENSITIVITY

/**
 * @brief DC bus voltage sensing ADC bias from the LVFB inverter voltage sensor.
 */
#define CTRL_DC_VOLTAGE_BIAS GMP_LVFB_VOLTAGE_BIAS_V

/**
 * @brief AC voltage sensing gain from the grid LC filter voltage sense path.
 */
#define CTRL_AC_VOLTAGE_SENSITIVITY (0.013559322f)

/**
 * @brief AC voltage sensing ADC bias from the grid LC filter.
 */
#define CTRL_AC_VOLTAGE_BIAS (1.65f)

/**
 * @brief AC current sensing sensitivity from the LVFB inverter current sensor.
 */
#define CTRL_AC_CURRENT_SENSITIVITY GMP_LVFB_CURRENT_SENSITIVITY

/**
 * @brief AC current sensing ADC bias from the LVFB inverter current sensor.
 */
#define CTRL_AC_CURRENT_BIAS GMP_LVFB_CURRENT_BIAS_V

/**
 * @brief Buck inductor-current sensing sensitivity from the lower LVFB half-bridge B5A current sensor.
 */
#define CTRL_BUCK_CURRENT_SENSITIVITY GMP_LVFB_CURRENT_SENSITIVITY

/**
 * @brief Buck inductor-current ADC bias from the lower LVFB half-bridge B5A current sensor.
 */
#define CTRL_BUCK_CURRENT_BIAS GMP_LVFB_CURRENT_BIAS_V

/**
 * @brief Buck output-voltage sensing gain from the LC filter board voltage sense path. This is separate from CTRL_DC_VOLTAGE_SENSITIVITY because the lower half-bridge voltage sensor measures its own DC bus, not Vo_buck.
 */
#define CTRL_BUCK_OUTPUT_VOLTAGE_SENSITIVITY (0.013559322f)

/**
 * @brief Buck output-voltage ADC bias from the LC filter board voltage sense path.
 */
#define CTRL_BUCK_OUTPUT_VOLTAGE_BIAS (1.65f)

/**
 * @brief Minimum PLL voltage magnitude used by P/Q reference division.
 */
#define CTRL_GRID_VMIN_PU (0.1f)

/**
 * @brief Maximum hardware DC bus voltage from the LVFB inverter board.
 */
#define CTRL_MAX_HW_VOLTAGE GMP_LVFB_VBUS_MAX_V

/**
 * @brief Maximum continuous RMS hardware current from the LVFB inverter board.
 */
#define CTRL_MAX_HW_CURRENT GMP_LVFB_CURRENT_MAX_RMS_A

/**
 * @brief Project DC bus over-voltage protection threshold.
 */
#define CTRL_PROT_VBUS_MAX (100.0f)

/**
 * @brief Fast AC peak-current trip threshold in A.
 */
#define CTRL_PROT_IAC_PEAK_MAX (CTRL_MAX_HW_CURRENT * 0.9f * 1.41421356f)

/**
 * @brief Maximum unsaturated modulation command before controller-divergence trip.
 */
#define CTRL_PROT_VCTRL_MAX_PU (1.5f)

/**
 * @brief Minimum physical DC-bus voltage accepted by the startup state machine.
 */
#define CTRL_DCBUS_READY_MIN (CTRL_DCBUS_VOLTAGE * 0.8f)

/**
 * @brief Maximum physical DC-bus voltage accepted by the startup state machine.
 */
#define CTRL_DCBUS_READY_MAX (CTRL_PROT_VBUS_MAX)

/**
 * @brief Single-phase PLL proportional gain.
 */
#define CTRL_PLL_KP (10.0f)

/**
 * @brief Single-phase PLL integral time constant in seconds.
 */
#define CTRL_PLL_TI (0.02f)

/**
 * @brief PLL q-axis error low-pass cutoff in Hz.
 */
#define CTRL_PLL_LPF_FC (20.0f)

/**
 * @brief Measured active/reactive power low-pass cutoff in Hz.
 */
#define CTRL_PQ_LPF_FC (200.0f)

/**
 * @brief Peak current-reference limit in per unit.
 */
#define CTRL_CURRENT_LIMIT_PU (1.5f)

/**
 * @brief Active-power command slew limit in PU/s.
 */
#define CTRL_P_SLEW_PU_S (10.0f)

/**
 * @brief Reactive-power command slew limit in PU/s.
 */
#define CTRL_Q_SLEW_PU_S (20.0f)

/**
 * @brief BUILD_LEVEL 5 target displacement power-factor magnitude. Valid control range is 0.1 to 1.0.
 */
#define SINV_POWER_FACTOR_REF (1.0f)

/**
 * @brief BUILD_LEVEL 5 reactive-power direction for PF control. Use +1 or -1 to select the quadrature-current direction.
 */
#define SINV_POWER_FACTOR_Q_SIGN (1.0f)

/**
 * @brief BUILD_LEVEL 5 PF-to-Q calibration gain. It only scales the reactive-power command converted from PF_ref, compensating measured Q/P deviation without changing PF_ref magnitude or Q direction.
 */
#define SINV_POWER_FACTOR_Q_GAIN (1.06f)

/**
 * @brief Current polarity deadband for PWM dead-time compensation.
 */
#define CTRL_CURRENT_DB_PU (0.01f)

/**
 * @brief QPR current-loop crossover target in Hz.
 */
#define SINV_CURRENT_LOOP_BANDWIDTH_HZ (600.0f)

/**
 * @brief Minimum fundamental frequency tracked by the repetitive controller in Hz.
 */
#define CTRL_FDRC_MIN_FREQ (45.0f)

/**
 * @brief Settling time before repetitive control starts learning.
 */
#define SINV_FDRC_ENABLE_DELAY_MS (300)

/**
 * @brief Frequency-adaptive repetitive-control learning gain.
 */
#define SINV_FDRC_LEARNING_GAIN (0.10f)

/**
 * @brief Frequency-adaptive repetitive-control robustness-filter cutoff frequency.
 */
#define SINV_FDRC_Q_FILTER_HZ (1000.0f)

/**
 * @brief Frequency-adaptive repetitive-control plant-delay compensation in controller samples.
 */
#define SINV_FDRC_LEAD_STEPS (3.0f)

/**
 * @brief Current-error threshold above which repetitive-control learning is frozen.
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
 * @brief ADC calibration timeout in ms.
 */
#define TIMEOUT_ADC_CALIB_MS (3000)

/**
 * @brief SPLL close-loop convergence criterion.
 */
#define CTRL_SPLL_EPSILON ((float2ctrl(0.005)))

/**
 * @brief Startup delay in ms.
 */
#define CTRL_STARTUP_DELAY (100)

/**
 * @brief Minimum operation-enabled transition delay used by the CiA402 startup sequence.
 */
#define SINV_CIA402_OPERATION_ENABLE_DELAY_MS (100)

/**
 * @brief Nominal AC grid/fundamental frequency in Hz.
 */
#define CTRL_GRID_FREQUENCY (50.0f)

/**
 * @brief Buck output voltage target.
 */
#define SINV_BUCK_OUTPUT_REF_V (48.0f)

/**
 * @brief BUILD_LEVEL 5 physical DC bus voltage target. This aliases CTRL_DCBUS_VOLTAGE so the DC-bus target follows the platform DC-bus voltage setting.
 */
#define SINV_DC_BUS_REF_V CTRL_DCBUS_VOLTAGE

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
 * @brief BUILD_LEVEL 6 signed grid active-power command used when SINV_LEVEL6_ENABLE_DCBUS_LOOP is 0. Positive exports power with the present P/Q sign convention; confirm the physical direction after current-sensor polarity is checked.
 */
#define SINV_LEVEL6_ACTIVE_POWER_REF_PU (0.10f)

/**
 * @brief BUILD_LEVEL 6 reactive-power command. It is used directly in both direct signed-P/Q mode and DC-bus-loop mode.
 */
#define SINV_LEVEL6_REACTIVE_POWER_REF_PU (0.0f)

/**
 * @brief BUILD_LEVEL 6 mode selector. 0: use SINV_LEVEL6_ACTIVE_POWER_REF_PU directly for bidirectional grid-current THD tests. 1: close the DC-bus voltage loop and let it generate signed active-power command.
 */
#define SINV_LEVEL6_ENABLE_DCBUS_LOOP (0)

/**
 * @brief BUILD_LEVEL 6 DC-bus-loop polarity from bus-voltage error to grid active-power command. Keep it separate from BUILD_LEVEL 5 so bidirectional power-flow sign calibration does not disturb the rectifier baseline.
 */
#define SINV_LEVEL6_DCBUS_POWER_SIGN (-1.0f)

/**
 * @brief Buck output-voltage reference soft-start slew rate in V/s. After Buck start conditions are met, the internal voltage reference ramps from 0 V to SINV_BUCK_OUTPUT_REF_V at this rate. This is the Buck soft-start parameter.
 */
#define SINV_BUCK_VREF_SLEW_V_S (120.0f)

/**
 * @brief Minimum DC bus voltage before Buck soft-start is allowed.
 */
#define SINV_BUCK_START_VBUS_MIN_V (55.0f)

/**
 * @brief Delay after the Buck start condition is met before PWM compare ramps.
 */
#define SINV_BUCK_START_DELAY_MS 100

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
#define SINV_BUCK_DUTY_FF_GAIN (0.15f)

/**
 * @brief Buck inductor-current command limit in ampere.
 */
#define SINV_BUCK_CURRENT_LIMIT_A (5.0f)

/**
 * @brief Buck voltage-loop execution frequency.
 */
#define SINV_BUCK_VOLTAGE_LOOP_FREQUENCY_HZ (200.0f)

/**
 * @brief Buck voltage-loop proportional gain.
 */
#define SINV_BUCK_VOLTAGE_LOOP_KP (0.1f)

/**
 * @brief Buck voltage-loop integral gain per second.
 */
#define SINV_BUCK_VOLTAGE_LOOP_KI (15.0f)

/**
 * @brief Buck current-loop proportional gain.
 */
#define SINV_BUCK_CURRENT_LOOP_KP (0.20f)

/**
 * @brief Buck current-loop integral gain per second.
 */
#define SINV_BUCK_CURRENT_LOOP_KI (500.0f)

// User project tail code
/* Compatibility with framework revisions that use the historical misspelling. */
#if defined(ENABLE_GMP_DL_PIL_SIM) && !defined(ENBALE_GMP_DL_PIL_SIM)
#define ENBALE_GMP_DL_PIL_SIM
#endif
#if defined(ENBALE_GMP_DL_PIL_SIM) && !defined(ENABLE_GMP_DL_PIL_SIM)
#define ENABLE_GMP_DL_PIL_SIM
#endif

#ifdef __cplusplus
}
#endif

#endif // _PROJECT_SDPE_PGS_SINV_RC_IRIS_BINDINGS_H_
