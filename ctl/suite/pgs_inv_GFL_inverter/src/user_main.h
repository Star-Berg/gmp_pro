//
// THIS IS A DEMO SOURCE CODE FOR GMP LIBRARY.
//
// User should add all declarations of user objects in this file.
//
// WARNING: This file must be kept in the include search path during compilation.
//

#include <core/dev/at_device.h>

#include <core/dev/display/ht16k33.h>
#include <core/pm/function_scheduler.h>

#ifndef _FILE_USER_MAIN_H_
#define _FILE_USER_MAIN_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=================================================================================================
// global variables

extern cia402_sm_t cia402_sm;
extern iic_halt iic_bus;
extern ht16k33_dev_t ui_keypad;

// HT16K33 repeats a held key after 120 ms. Treat each supported key press as
// one event and re-arm only after the key has been released.
#define GFL_UI_KEY_RELEASE_TIMEOUT_MS (200U)

extern volatile uint16_t ui_last_key;
extern volatile uint16_t ui_fault_code;
extern volatile int16_t ui_keypad_ec;
extern volatile float ui_dc_bus_voltage;
extern volatile uint16_t ui_initialized;

#ifndef SPECIFY_PC_TEST_ENV

#endif // SPECIFY_PC_TEST_ENV

//=================================================================================================
// global functions

//
// User should implement this 3 functions at least
//
void init(void);
void mainloop(void);
void setup_peripheral(void);

//
// For Controller projects user should implement the following functions
//
void ctl_init(void);
void ctl_mainloop(void);

gmp_task_status_t tsk_startup(gmp_task_t* tsk);
gmp_task_status_t tsk_keyboard(gmp_task_t* tsk);
gmp_task_status_t tsk_oled(gmp_task_t* tsk);

#ifdef __cplusplus
}
#endif

#endif // _FILE_USER_MAIN_H_
