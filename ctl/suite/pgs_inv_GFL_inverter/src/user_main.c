// This is the example of user main.

// GMP basic core header
#include <gmp_core.h>

// user main header
#include "user_main.h"

#include <core/dev/mem_presp.h>
#include <core/dev/pil_core.h>
#include <core/dev/tunable.h>
#include <stdio.h>

#ifdef IRIS_IIC_BASE
#include <oled_driver.h>
#define GFL_IRIS_PANEL_ENABLED (1)
#else
#define GFL_IRIS_PANEL_ENABLED (0)
#endif

//=================================================================================================
// Datalink protocol online Debug module

gmp_datalink_t dl;

//
// PIL (processor in loop module)
//
gmp_pil_sim_t pil;

// IRIS local operator panel
iic_halt iic_bus;
ht16k33_dev_t ui_keypad;
volatile uint16_t ui_last_key = 0;
volatile uint16_t ui_fault_code = 0;
volatile int16_t ui_keypad_ec = GMP_EC_OK;
volatile float ui_dc_bus_voltage = 0.0f;
volatile uint16_t ui_initialized = 0;

//
// Tunable Dictionary
//
const gmp_param_item_t dict_m1[] = {
    {&cia402_sm.current_cmd, GMP_PARAM_TYPE_U16, GMP_PARAM_PERM_RW},
    {&cia402_sm.current_state, GMP_PARAM_TYPE_U16, GMP_PARAM_PERM_RO},
    {(void*)&ctl_user_run_request, GMP_PARAM_TYPE_U16, GMP_PARAM_PERM_RW},
    {(void*)&ctl_output_frequency_hz, GMP_PARAM_TYPE_U16, GMP_PARAM_PERM_RO},
    {(void*)&ctl_output_frequency_request_hz, GMP_PARAM_TYPE_U16, GMP_PARAM_PERM_RO},
    {(void*)&ctl_dc_bus_voltage_setting_v, GMP_PARAM_TYPE_U16, GMP_PARAM_PERM_RO},
    {(void*)&ctl_dc_bus_voltage_request_v, GMP_PARAM_TYPE_U16, GMP_PARAM_PERM_RO},
    {&inv_ctrl.filter_udc.out, GMP_PARAM_TYPE_F32, GMP_PARAM_PERM_RO},
    {(void*)&ui_dc_bus_voltage, GMP_PARAM_TYPE_F32, GMP_PARAM_PERM_RO},
    {(void*)&ui_last_key, GMP_PARAM_TYPE_U16, GMP_PARAM_PERM_RO},
    {(void*)&ui_fault_code, GMP_PARAM_TYPE_U16, GMP_PARAM_PERM_RO},
    {(void*)&ui_keypad_ec, GMP_PARAM_TYPE_I16, GMP_PARAM_PERM_RO},
    {(void*)&ui_initialized, GMP_PARAM_TYPE_U16, GMP_PARAM_PERM_RO},
    {&inv_ctrl.idq_set.dat[phase_d], GMP_PARAM_TYPE_F32, GMP_PARAM_PERM_RW},
    {&inv_ctrl.idq_set.dat[phase_q], GMP_PARAM_TYPE_F32, GMP_PARAM_PERM_RW},
    {&inv_ctrl.idq.dat[phase_d], GMP_PARAM_TYPE_F32, GMP_PARAM_PERM_RO},
    {&inv_ctrl.idq.dat[phase_q], GMP_PARAM_TYPE_F32, GMP_PARAM_PERM_RO},
    {&pq_ctrl.pq_set.dat[0], GMP_PARAM_TYPE_F32, GMP_PARAM_PERM_RW},
    {&pq_ctrl.pq_set.dat[1], GMP_PARAM_TYPE_F32, GMP_PARAM_PERM_RW},
    {&pq_ctrl.pq_meas.dat[0], GMP_PARAM_TYPE_F32, GMP_PARAM_PERM_RO},
    {&pq_ctrl.pq_meas.dat[1], GMP_PARAM_TYPE_F32, GMP_PARAM_PERM_RO},
};
const uint16_t var_tunable_count = sizeof(dict_m1) / sizeof(dict_m1[0]);
gmp_param_tunable_t tunable;

//
// Memory perspective Dictionary
//
const gmp_mem_region_t mem_regions[] = {
    {.base_addr = &inv_ctrl, .byte_length = sizeof(inv_ctrl) * GMP_PORT_DATA_SIZE_PER_BYTES, .perm = GMP_MEM_PERM_RW},
};
const uint16_t mem_regions_count = sizeof(mem_regions) / sizeof(mem_regions[0]);
gmp_mem_persp_t mem_persp_server;

//
// Datalink protocol stack task
//
gmp_task_status_t tsk_dl_debug_device(gmp_task_t* tsk)
{
    GMP_UNUSED_VAR(tsk);

    // In PC simulation environment the DL protocol module is disabled.
#ifndef SPECIFY_PC_ENVIRONMENT

    flush_dl_rx_buffer();

    gmp_dl_event_t e = gmp_dev_dl_loop_cb(&dl);

    switch (e)
    {
    //
    // if TX data is ready, do transmit
    //
    case GMP_DL_EVENT_TX_RDY:

        // send tx buffer message
        flush_dl_tx_buffer();

        // ack TX state machine.
        gmp_dev_dl_tx_state_done(&dl);
        break;

    case GMP_DL_EVENT_RX_OK:

        //
        // Ack PIL simulation message
        //
        if (gmp_pil_sim_rx_cb(&pil))
            break;

        //
        // Ack parameter tunable message
        //
        if (gmp_param_tunable_rx_cb(&tunable))
            break;

        //
        // Ack memory perspective message
        //
        if (gmp_mem_persp_rx_cb(&mem_persp_server))
            break;

        //
        // Echo Command
        //
        if (dl.rx_head.cmd == 0x99)
        {
            // echo payload_buf
            gmp_dev_dl_tx_request(&dl, dl.rx_head.seq_id, GMP_DL_CMD_ECHO, dl.expected_payload_len, dl.payload_buf);

            // ack this message
            gmp_dev_dl_msg_handled(&dl);

            break;
        }

        // default handler
        gmp_dev_dl_default_rx_handler(&dl);

        break;
    }

#endif // SPECIFY_PC_ENVIRONMENT

    return GMP_TASK_DONE;
}

//=================================================================================================
// task manager

// GPIO
gpio_halt user_led;


gmp_task_status_t tsk_blink(gmp_task_t* tsk)
{
    GMP_UNUSED_VAR(tsk);

    gmp_base_print(TEXT_STRING("Hello World!\r\n"));

    static fast_gt led_stat = 0;
    if (led_stat == 0)
    {
        led_stat = 1;
        gmp_hal_gpio_write(user_led, 0);
    }
    else
    {
        led_stat = 0;
        gmp_hal_gpio_write(user_led, 1);
    }

    return GMP_TASK_DONE;
}

void send_monitor_data(void);
gmp_task_status_t tsk_monitor(gmp_task_t* tsk)
{
    GMP_UNUSED_VAR(tsk);

    send_monitor_data();

    return GMP_TASK_DONE;
}

static uint16_t ui_get_fault_code(void)
{
    if (ui_keypad_ec != GMP_EC_OK)
        return 1U;

    if ((cia402_sm.current_state == CIA402_SM_FAULT_REACTION) ||
        (cia402_sm.current_state == CIA402_SM_FAULT))
        return 2U;

    if (cia402_sm.last_cb_result <= CIA402_EC_ERROR)
        return 3U;

    return 0U;
}

#if GFL_IRIS_PANEL_ENABLED
static const data_gt ui_7segment_digit_lut[10] = {
    0x3F, // 0
    0x06, // 1
    0x5B, // 2
    0x4F, // 3
    0x66, // 4
    0x6D, // 5
    0x7D, // 6
    0x07, // 7
    0x7F, // 8
    0x6F, // 9
};

#define UI_7SEG_L     ((data_gt)0x38)
#define UI_7SEG_F     ((data_gt)0x71)
#define UI_7SEG_DASH  ((data_gt)0x40)
#define UI_7SEG_BLANK ((data_gt)0x00)

static uint16_t ui_7segment_frequency_hz = 0U;

static void ui_set_7segment_frequency(uint16_t frequency_hz)
{
    uint16_t i;

    for (i = 0U; i < HT16K33_CFG_DISP_RAM_SIZE; ++i)
        ui_keypad.display_ram[i] = UI_7SEG_BLANK;

    // Eight digits: L<level>-F-<frequency>, for example L7-F-60.
    ui_keypad.display_ram[0] = UI_7SEG_L;
    ui_keypad.display_ram[2] = ui_7segment_digit_lut[BUILD_LEVEL % 10U];
    ui_keypad.display_ram[4] = UI_7SEG_DASH;
    ui_keypad.display_ram[6] = UI_7SEG_F;
    ui_keypad.display_ram[8] = UI_7SEG_DASH;
    ui_keypad.display_ram[10] = ui_7segment_digit_lut[(frequency_hz / 10U) % 10U];
    ui_keypad.display_ram[12] = ui_7segment_digit_lut[frequency_hz % 10U];
    ui_keypad.display_ram[14] = UI_7SEG_BLANK;
    ui_keypad.is_dirty = 1;
    ui_7segment_frequency_hz = frequency_hz;
}

static void ui_oled_write_line(uint8_t page, const char* text)
{
    static char last_line[4][17] = {{0}};
    char line[17];
    uint16_t line_index = (uint16_t)(page >> 1);
    uint16_t i = 0;
    fast_gt changed = 0;

    while (i < 16U)
    {
        line[i] = ' ';
        ++i;
    }

    i = 0;
    while ((i < 16U) && (text[i] != '\0'))
    {
        line[i] = text[i];
        ++i;
    }
    line[16] = '\0';

    if (line_index < 4U)
    {
        for (i = 0; i < 17U; ++i)
        {
            if (last_line[line_index][i] != line[i])
            {
                changed = 1;
                break;
            }
        }

        if (!changed)
            return;

        for (i = 0; i < 17U; ++i)
            last_line[line_index][i] = line[i];
    }

    oled_show_str(0, page, line);
}
#endif

gmp_task_status_t tsk_keyboard(gmp_task_t* tsk)
{
    static fast_gt ui_key_armed = 1;
    static time_gt ui_key_last_event = 0;
    fast_gt key_id = 0;
    ec_gt ret;

    GMP_UNUSED_VAR(tsk);

    if (!ui_initialized || (ui_keypad_ec != GMP_EC_OK))
        return GMP_TASK_DONE;

    ret = ht16k33_read_keys(&ui_keypad, &key_id);
    if (ret != GMP_EC_OK)
    {
        ui_keypad_ec = (int16_t)ret;
        return GMP_TASK_DONE;
    }

    if (key_id == 0)
    {
        if (!ui_key_armed &&
            (gmp_base_get_diff_system_tick(ui_key_last_event) >
             GFL_UI_KEY_RELEASE_TIMEOUT_MS))
            ui_key_armed = 1;

        return GMP_TASK_DONE;
    }

    // Only the four configured operator keys are active.
    if ((key_id != GFL_UI_KEY_SWITCH_ID) && (key_id != GFL_UI_KEY_FREQUENCY_ID) &&
        (key_id != GFL_UI_KEY_FAULT_RESET_ID) && (key_id != GFL_UI_KEY_DCBUS_ID))
        return GMP_TASK_DONE;

    ui_last_key = (uint16_t)key_id;
    ui_key_last_event = gmp_base_get_system_tick();

    if (!ui_key_armed)
        return GMP_TASK_DONE;

    ui_key_armed = 0;
    switch (key_id)
    {
    case GFL_UI_KEY_SWITCH_ID:
        ctl_set_run_request(ctl_user_run_request ? 0 : 1);
        break;

    case GFL_UI_KEY_FREQUENCY_ID:
    {
        uint16_t target_frequency_hz = (ctl_output_frequency_hz == 30U) ? 60U : 30U;
        ctl_request_output_frequency_hz(target_frequency_hz);
        break;
    }

    case GFL_UI_KEY_FAULT_RESET_ID:
        ctl_set_run_request(0);
        cia402_fault_reset(&cia402_sm);
        break;

    case GFL_UI_KEY_DCBUS_ID:
    {
        uint16_t target_dc_bus_voltage_v =
            (ctl_dc_bus_voltage_setting_v == GFL_UI_DCBUS_LOW_V)
                ? GFL_UI_DCBUS_HIGH_V
                : GFL_UI_DCBUS_LOW_V;
        ctl_request_dc_bus_voltage_v(target_dc_bus_voltage_v);
        break;
    }

    default:
        break;
    }

    gmp_base_print("Panel key %u, run request %u\r\n", (unsigned int)key_id,
                   (unsigned int)ctl_user_run_request);
    return GMP_TASK_DONE;
}

gmp_task_status_t tsk_oled(gmp_task_t* tsk)
{
    GMP_UNUSED_VAR(tsk);

    ui_dc_bus_voltage = ctrl2float(inv_ctrl.filter_udc.out) * (float)CTRL_VOLTAGE_BASE;
    ui_fault_code = ui_get_fault_code();

#if GFL_IRIS_PANEL_ENABLED
    if (ui_initialized)
    {
        char text[24];
        uint16_t dc_bus_decivolts;

        if (ui_dc_bus_voltage < 0.0f)
            dc_bus_decivolts = 0U;
        else if (ui_dc_bus_voltage > 999.9f)
            dc_bus_decivolts = 9999U;
        else
            dc_bus_decivolts = (uint16_t)(ui_dc_bus_voltage * 10.0f + 0.5f);

        sprintf(text, "FREQ SET:%02u Hz", (unsigned int)ctl_output_frequency_hz);
        ui_oled_write_line(0, text);

        sprintf(text, "DC BUS:%3u.%u V", (unsigned int)(dc_bus_decivolts / 10U),
                (unsigned int)(dc_bus_decivolts % 10U));
        ui_oled_write_line(2, text);

        sprintf(text, "OUTPUT:%s", ctl_user_run_request ? "ON" : "OFF");
        ui_oled_write_line(4, text);

        sprintf(text, "BUS SET:%2u V", (unsigned int)ctl_dc_bus_voltage_setting_v);
        ui_oled_write_line(6, text);

        if ((ui_keypad_ec == GMP_EC_OK) &&
            (ui_7segment_frequency_hz != ctl_output_frequency_hz))
        {
            ui_set_7segment_frequency(ctl_output_frequency_hz);
            ui_keypad_ec = (int16_t)ht16k33_update_display(&ui_keypad);
        }
    }
#endif

    return GMP_TASK_DONE;
}

//
// Non-blocking task scheduler
//
gmp_scheduler_t sched;

// tasks list
gmp_task_t tasks[] = {
    // name,     task,      period(ms),  init_phase, is_enabled, pParam
    {"startup", tsk_startup, 100, 0, 1, NULL},
    {"keyboard", tsk_keyboard, 50, 10, 1, NULL},
    {"oled", tsk_oled, 250, 20, 1, NULL},
    {"blink_led", tsk_blink, 1000, 0, 1, NULL},
    {"dl_online", tsk_dl_debug_device, 2, 0, 1, NULL},
    {"monitor_data", tsk_monitor, 2, 0, 1, NULL},
};

//=================================================================================================
// initialize routine

GMP_NO_OPT_PREFIX
void init(void) GMP_NO_OPT_SUFFIX
{
    int i;

    // init scheduler
    gmp_scheduler_init(&sched);

    for (i = 0; i < sizeof(tasks) / sizeof(gmp_task_t); ++i)
        gmp_scheduler_add_task(&sched, &tasks[i]);

    // init datalink protocol
    gmp_dev_dl_init(&dl);

    // enable PIL simulation environment
    gmp_pil_sim_init(&pil, &dl, 0x10);

    // Band DL module with tunable and persp module.
    gmp_param_tunable_init(&tunable, &dl, 0x30, dict_m1, var_tunable_count);
    gmp_mem_persp_init(&mem_persp_server, &dl, 0x50, mem_regions, mem_regions_count);
}

// Initialization tasks after all peripherals have been initialized
gmp_task_status_t tsk_startup(gmp_task_t* tsk)
{
    ctl_set_run_request(0);

#if GFL_IRIS_PANEL_ENABLED
    {
        ht16k33_init_t keypad_cfg = {
            .brightness = 8,
            .blink_rate = 0,
            .int_enable = 0,
            .int_act_high = 0,
        };
        ec_gt ret;

        iic_bus = IRIS_IIC_BASE;
        ret = ht16k33_init(&ui_keypad, iic_bus, HT16K33_DEFAULT_DEV_ADDR, &keypad_cfg);
        ui_keypad_ec = (int16_t)ret;
        if (ret == GMP_EC_OK)
        {
            ui_set_7segment_frequency(ctl_output_frequency_hz);
            ret = ht16k33_update_display(&ui_keypad);
            ui_keypad_ec = (int16_t)ret;
        }

        oled_init();
        oled_clear();
        ui_initialized = 1;
    }
#else
    ui_keypad_ec = GMP_EC_NOT_READY;
#endif

    // Startup is complete; PWM remains disabled until the RUN key is pressed.
    tsk->is_enabled = 0;

    return GMP_TASK_DONE;
}

//=================================================================================================
// endless loop routine

GMP_NO_OPT_PREFIX
void mainloop(void) GMP_NO_OPT_SUFFIX
{
    // run task scheduler
    gmp_scheduler_dispatch(&sched);
}
