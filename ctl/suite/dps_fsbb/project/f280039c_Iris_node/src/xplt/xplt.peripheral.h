
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
#define CONTROLLER_LED SYSTEM_LED
#endif // CONTROLLER_LED

// select ADC PTR interface
#include <ctl/component/interface/adc_ptr_channel.h>

#include <core/dev/datalink.h>

// Number of raw VOUT ADC conversions used by the trimmed moving average that
// feeds the voltage-control path. With the default 8-point window, one maximum
// and one minimum sample are rejected and the remaining six are averaged.
// Set to 1 to compile without averaging.
#ifndef FSBB_VOUT_ADC_AVERAGE_SAMPLES
#define FSBB_VOUT_ADC_AVERAGE_SAMPLES (8U)
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

// VOUT ADC moving-average diagnostics. These can be added directly to the
// CCS Expressions/Graph views without using calculated expressions.
extern volatile uint16_t g_fsbb_vout_adc_raw;
extern volatile uint16_t g_fsbb_vout_adc_average;
extern volatile uint16_t g_fsbb_vout_adc_window_min;
extern volatile uint16_t g_fsbb_vout_adc_window_max;
extern volatile fast_gt g_fsbb_vout_adc_average_enable;
extern volatile fast_gt g_fsbb_vout_adc_trim_enable;

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
