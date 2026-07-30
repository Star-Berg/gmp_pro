
//
// THIS IS A DEMO SOURCE CODE FOR GMP LIBRARY.
//
// User should add all necessary GMP config macro in this file.
//
// WARNING: This file must be kept in the include search path during compilation.
//

#ifndef _FILE_XPLT_PERIPHERAL_H_
#define _FILE_XPLT_PERIPHERAL_H_

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

#include <gmp_core.h>

// SDPE-generated controller settings
#include <sdpe_dps_fsbb_iris_settings.h>

#ifndef CONTROLLER_LED
#define CONTROLLER_LED IRIS_LED2
#endif // CONTROLLER_LED

// GPIO58/IRIS_GPIO1 is the FSBB power-stage Enable_uC output. The reference
// panel project used the same pin for its buzzer, but that assignment is not
// valid when the FSBB power stage is connected.
#if defined(PSU_BEEP_PORT) && (PSU_BEEP_PORT == PWM_ENABLE_PORT)
#error "PSU_BEEP_PORT conflicts with the FSBB gate-driver enable GPIO"
#endif

#if (PWM_ENABLE_PORT == PWM_RESET_PORT)
#error "FSBB gate-driver enable and reset must use different GPIOs"
#endif

#if (PWM_ENABLE_PORT == SYSTEM_LED) || (PWM_ENABLE_PORT == CONTROLLER_LED)
#error "FSBB status LED conflicts with the gate-driver enable GPIO"
#endif

// select ADC PTR interface
#include <ctl/component/interface/adc_ptr_channel.h>

#include <core/dev/datalink.h>

// Number of raw VIN/VOUT ADC conversions used by the moving averages. Set
// either value to 1 to compile that channel without averaging.
#ifndef FSBB_VIN_ADC_AVERAGE_SAMPLES
#define FSBB_VIN_ADC_AVERAGE_SAMPLES (64U)
#endif

#ifndef FSBB_VOUT_ADC_AVERAGE_SAMPLES
#define FSBB_VOUT_ADC_AVERAGE_SAMPLES (8U)
#endif

#if (FSBB_VIN_ADC_AVERAGE_SAMPLES < 1U) || (FSBB_VIN_ADC_AVERAGE_SAMPLES > 256U)
#error "FSBB_VIN_ADC_AVERAGE_SAMPLES must be in the range 1..256"
#endif

#if (FSBB_VOUT_ADC_AVERAGE_SAMPLES < 1U) || (FSBB_VOUT_ADC_AVERAGE_SAMPLES > 256U)
#error "FSBB_VOUT_ADC_AVERAGE_SAMPLES must be in the range 1..256"
#endif

//=================================================================================================
// definitions of peripheral

extern adc_channel_t adc_v_in;
extern adc_channel_t adc_v_out;
extern adc_channel_t adc_i_L;
extern adc_channel_t adc_i_load;

// VIN ADC moving-average diagnostics. These can be added directly to the
// CCS Expressions/Graph views without using calculated expressions.
extern volatile uint16_t g_fsbb_vin_adc_raw;
extern volatile uint16_t g_fsbb_vin_adc_average;
extern volatile fast_gt g_fsbb_vin_adc_average_enable;

// VOUT ADC moving-average diagnostics. These can be added directly to the
// CCS Expressions/Graph views without using calculated expressions.
extern volatile uint16_t g_fsbb_vout_adc_raw;
extern volatile uint16_t g_fsbb_vout_adc_average;
extern volatile fast_gt g_fsbb_vout_adc_average_enable;

uint16_t fsbb_average_vin_adc_sample(uint16_t sample);
uint16_t fsbb_average_vout_adc_sample(uint16_t sample);

// dlog DSA objects
//extern basic_trigger_t trigger;

#define DLOG_MEM_LENGTH 100
extern ctrl_gt dlog_mem1[DLOG_MEM_LENGTH];
extern ctrl_gt dlog_mem2[DLOG_MEM_LENGTH];

void reset_controller(void);
void flush_dl_tx_buffer(void);
void flush_dl_rx_buffer(void);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _FILE_PERIPHERAL_H_
