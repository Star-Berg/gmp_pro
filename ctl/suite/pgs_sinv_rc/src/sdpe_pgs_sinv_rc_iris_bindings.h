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
#include <sdpe_pgs_sinv_rc_common_settings.h>
/* Project-specific hardware bindings follow the shared SINV control contract. */

//=================================================================================================
/**
 * @brief Project metadata.
 */

#define SDPE_PROJECT_ID "pgs_sinv_rc_iris_node"
#define SDPE_PROJECT_SUITE "pgs_sinv_rc"
#define SDPE_PROJECT_VERSION "0.2.0"
#define SDPE_PROJECT_UPDATED_AT "2026-07-30"

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
// #define SPECIFY_ENABLE_ADC_CALIBRATE

//=================================================================================================
/**
 * @brief Control Features.
 */

/**
 * @brief Allow ENABLE_OPERATION to advance through the complete CiA402 startup sequence.
 */
#define CIA402_CONFIG_ENABLE_SEQUENCE_SWITCH

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
 *        BUILD_LEVEL 5: original rectifier DC-bus voltage loop with Q_ref = 0 and no Buck/Boost PWM.
 *        BUILD_LEVEL 6: rectifier DC-bus voltage loop with PF-derived Q command and downstream Buck control.
 *        BUILD_LEVEL 7: boost-fed grid mode; the DCDC stage regulates DC bus from the low-voltage side, while the grid side uses direct signed P/Q.
 *        Options: (1), (2), (3), (4), (5), (6), (7)
 */
#define BUILD_LEVEL (5)

//=================================================================================================
/**
 * @brief Rectifier Keyboard Control.
 */

/**
 * @brief Enable HT16K33 keyboard control on the rectifier DSP.
 *        Options: (0), (1)
 */
#define SINV_KEYBOARD_CONTROL_ENABLE (1)

/**
 * @brief HT16K33 key ID used by SW1. SW1 toggles CiA402 enable/disable.
 */
#define SINV_KEYBOARD_SW1_KEY_ID (16U)

/**
 * @brief HT16K33 key ID used by SW2. SW2 toggles the DC-bus reference profile.
 */
#define SINV_KEYBOARD_SW2_KEY_ID (3U)

/**
 * @brief HT16K33 key ID used by SW3. SW3 cancels startup or resets a CiA402 fault.
 */
#define SINV_KEYBOARD_SW3_KEY_ID (1U)

/**
 * @brief SW2.0 DC-bus reference in V. This is the default efficiency-test profile.
 */
#define SINV_KEYBOARD_VBUS_REF_0_V (50.0f)

/**
 * @brief SW2.1 DC-bus reference in V. This is the higher-bus profile for high input voltage tests.
 */
#define SINV_KEYBOARD_VBUS_REF_1_V (60.0f)

/**
 * @brief Default SW2 profile after power-up. 0 selects SINV_KEYBOARD_VBUS_REF_0_V.
 *        Options: (0U), (1U)
 */
#define SINV_KEYBOARD_DEFAULT_VBUS_PROFILE (0U)

/**
 * @brief Keyboard scan period in ms.
 */
#define SINV_KEYBOARD_SCAN_PERIOD_MS (50U)

/**
 * @brief Required time without repeated key messages before another key action is accepted.
 */
#define SINV_KEYBOARD_RELEASE_TIMEOUT_MS (200U)

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
#define PWM_ENABLE_PORT IRIS_GPIO5

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
#define CTRL_DCBUS_VOLTAGE (65.0f)

/**
 * @brief Rated AC grid/load RMS voltage.
 */
#define CTRL_GRID_VOLTAGE_RMS (36.0f)

/**
 * @brief Rated AC output RMS current.
 */
#define CTRL_RATED_CURRENT_RMS (10.0f)

/**
 * @brief Voltage per-unit base, using peak value.
 */
#define CTRL_VOLTAGE_BASE (51.0f)

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
#define CTRL_DC_VOLTAGE_SENSITIVITY (0.0271f)

/**
 * @brief DC bus voltage sensing ADC bias from the LVFB inverter voltage sensor.
 */
#define CTRL_DC_VOLTAGE_BIAS (0.01f)

/**
 * @brief AC voltage sensing gain from the grid LC filter voltage sense path.
 */
#define CTRL_AC_VOLTAGE_SENSITIVITY (0.0132421429f)

/**
 * @brief AC voltage sensing ADC bias from the grid LC filter.
 */
#define CTRL_AC_VOLTAGE_BIAS (1.6582785714f)

/**
 * @brief AC current sensing sensitivity from the LVFB inverter current sensor. The negative sign matches the installed sensor direction.
 */
#define CTRL_AC_CURRENT_SENSITIVITY (-0.1481912172f)

/**
 * @brief AC current sensing ADC bias from the LVFB inverter current sensor.
 */
#define CTRL_AC_CURRENT_BIAS (1.6568447327f)

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
 * @brief Minimum physical DC-bus voltage accepted by startup. BUILD_LEVEL 5 accepts passive precharge from the 24 Vrms grid; other build levels retain the 80 percent DC-bus requirement.
 */
#define CTRL_DCBUS_READY_MIN ((BUILD_LEVEL == 5) ? 25.0f : (CTRL_DCBUS_VOLTAGE * 0.8f))

/**
 * @brief Maximum physical DC-bus voltage accepted by the startup state machine.
 */
#define CTRL_DCBUS_READY_MAX (CTRL_PROT_VBUS_MAX)

/**
 * @brief Peak current-reference limit in per unit.
 */
#define CTRL_CURRENT_LIMIT_PU (0.6f)

/**
 * @brief Active-power command slew limit in PU/s.
 */
#define CTRL_P_SLEW_PU_S (10.0f)

/**
 * @brief Reactive-power command slew limit in PU/s.
 */
#define CTRL_Q_SLEW_PU_S (20.0f)

/**
 * @brief BUILD_LEVEL 6 reactive-power direction for PF control. Use +1 or -1 to select the quadrature-current direction.
 */
#define SINV_POWER_FACTOR_Q_SIGN (1.0f)

/**
 * @brief ADC calibration timeout in ms.
 */
#define TIMEOUT_ADC_CALIB_MS (3000)

/**
 * @brief BUILD_LEVEL 7 grid RMS voltage reference used by the RMS protection window. This is a target/nominal value; the PLL still uses the measured grid voltage.
 */
#define SINV_LEVEL7_GRID_VOLTAGE_RMS (24.0f)

/**
 * @brief BUILD_LEVEL 7 Boost target DC-bus voltage. In level 7 the DCDC stage regulates Vbus from the low-voltage input side.
 */
#define SINV_LEVEL7_DC_BUS_REF_V (60.0f)

/**
 * @brief BUILD_LEVEL 7 nominal low-voltage DC source value feeding the Boost input side.
 */
#define SINV_LEVEL7_BOOST_INPUT_REF_V (48.0f)

/**
 * @brief BUILD_LEVEL 7 Boost DC-bus reference soft-start slew rate in V/s.
 */
#define SINV_LEVEL7_BOOST_VBUS_SLEW_V_S (120.0f)

/**
 * @brief BUILD_LEVEL 7 delay after the low-voltage Boost input is present and CiA402 is operation-enabled before Boost PWM starts.
 */
#define SINV_LEVEL7_BOOST_START_DELAY_MS (100)

/**
 * @brief Startup delay in ms.
 */
#define CTRL_STARTUP_DELAY (100)

/**
 * @brief Buck output voltage target.
 */
#define SINV_BUCK_OUTPUT_REF_V (48.0f)

/**
 * @brief BUILD_LEVEL 5 physical DC bus voltage target. This aliases CTRL_DCBUS_VOLTAGE so the DC-bus target follows the platform DC-bus voltage setting.
 */
#define SINV_DC_BUS_REF_V CTRL_DCBUS_VOLTAGE

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
