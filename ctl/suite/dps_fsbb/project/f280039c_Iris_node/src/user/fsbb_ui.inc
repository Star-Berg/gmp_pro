/**
 * @file fsbb_ui.inc
 * @brief Non-blocking HT16K33 keypad/7-segment and OLED display for the FSBB target.
 *
 * Key IDs, task periods, release debounce, display addressing and I2C error
 * handling are migrated from D:\Github\albemuth\iris_280039c_board.
 * This first-stage UI deliberately changes only the existing voltage command
 * and current-limit command. AUTO/CV/CC mode selection remains reserved until
 * the three-mode controller interface is present.
 *
 * Buzzer control is intentionally not migrated. IRIS_GPIO1/GPIO58 is reserved
 * by the FSBB target for the power-stage Enable_uC signal and must not also be
 * written as PSU_BEEP_PORT.
 */

#include <gmp_core.h>
#include <stdio.h>

#include <core/dev/display/ht16k33.h>
#include "ctl_main.h"

//=================================================================================================
// Hardware and interaction contract migrated from the verified Iris panel project

#define FSBB_UI_KEY_DIGIT_1_ID      (15U)
#define FSBB_UI_KEY_DIGIT_2_ID      (14U)
#define FSBB_UI_KEY_DIGIT_3_ID      (22U)
#define FSBB_UI_KEY_DIGIT_4_ID      (6U)
#define FSBB_UI_KEY_DIGIT_5_ID      (5U)
#define FSBB_UI_KEY_DIGIT_6_ID      (4U)
#define FSBB_UI_KEY_DIGIT_7_ID      (19U)
#define FSBB_UI_KEY_DIGIT_8_ID      (18U)
#define FSBB_UI_KEY_DIGIT_9_ID      (17U)
#define FSBB_UI_KEY_DIGIT_0_ID      (2U)
#define FSBB_UI_KEY_DECIMAL_ID      (1U)

#define FSBB_UI_KEY_CONFIRM_ID      (9U)
#define FSBB_UI_KEY_MODE_ID         (7U)
#define FSBB_UI_KEY_OUTPUT_ID       (16U)
#define FSBB_UI_KEY_CLEAR_ID        (21U)
#define FSBB_UI_KEY_BACKSPACE_ID    (8U)
#define FSBB_UI_KEY_EDIT_TOGGLE_ID  (3U)

#define FSBB_UI_KEY_RELEASE_TICKS   (4U)
#define FSBB_UI_INPUT_TIMEOUT_TICKS (200U)
#define FSBB_UI_INPUT_BUFFER_SIZE   (8U)

#define FSBB_UI_ENCODER_BASE                 (IRIS_EQEP1_BASE)
#define FSBB_UI_ENCODER_MAX_POSITION         (10000UL)
#define FSBB_UI_ENCODER_COUNTS_PER_STEP      (4L)
#define FSBB_UI_ENCODER_REVERSE              (0U)
#define FSBB_UI_ENCODER_BUTTON_PORT          (IRIS_EQEP1_EQEPINDEX_GPIO)
#define FSBB_UI_ENCODER_BUTTON_ACTIVE_LEVEL  (0U)
#define FSBB_UI_ENCODER_BUTTON_RELEASE_TICKS (2U)

// Encoder increments are stored in hundredths of the physical unit so the
// user setting remains deterministic after repeated clockwise/counter-clockwise
// adjustments. Voltage: 0.10/0.50 V; current limit: 0.01/0.10 A.
#define FSBB_UI_ENCODER_V_FINE_STEP_CV   (10L)
#define FSBB_UI_ENCODER_V_COARSE_STEP_CV (50L)
#define FSBB_UI_ENCODER_I_FINE_STEP_CA   (1L)
#define FSBB_UI_ENCODER_I_COARSE_STEP_CA (10L)

// The physical user ranges come from the converter specification. They are
// kept in this UI-only stage until the guide's SDPE user-range macros exist.
#define FSBB_UI_VSET_MIN_V (10.0f)
#define FSBB_UI_VSET_MAX_V (50.0f)
#define FSBB_UI_ISET_MIN_A (1.0f)
#define FSBB_UI_ISET_MAX_A (5.0f)

// Display-only moving averages. These run in the 50 ms UI task and never
// affect ADC feedback, protection, or PWM control. Output current receives a
// longer window because its sensor path is visibly noisier than VOUT.
#define FSBB_UI_VOUT_DISPLAY_AVERAGE_SAMPLES (16U)
#define FSBB_UI_IOUT_DISPLAY_AVERAGE_SAMPLES (64U)

#if (FSBB_UI_VOUT_DISPLAY_AVERAGE_SAMPLES < 1U) || (FSBB_UI_VOUT_DISPLAY_AVERAGE_SAMPLES > 64U)
#error "FSBB_UI_VOUT_DISPLAY_AVERAGE_SAMPLES must be in the range 1..64"
#endif

#if (FSBB_UI_IOUT_DISPLAY_AVERAGE_SAMPLES < 1U) || (FSBB_UI_IOUT_DISPLAY_AVERAGE_SAMPLES > 64U)
#error "FSBB_UI_IOUT_DISPLAY_AVERAGE_SAMPLES must be in the range 1..64"
#endif

#define FSBB_UI_VOUT_DISPLAY_ZERO_V (0.5f)
#define FSBB_UI_IOUT_DISPLAY_ZERO_A (0.10f)

#define FSBB_OLED_I2C_ADDRESS    (0x3CU)
#define FSBB_OLED_WIDTH          (128U)
#define FSBB_OLED_PAGE_COUNT     (8U)
#define FSBB_OLED_LINE_CHARS     (21U)
#define FSBB_OLED_I2C_TIMEOUT_MS (40U)

typedef enum _tag_fsbb_ui_edit_target_t
{
    FSBB_UI_EDIT_VOLTAGE = 0,
    FSBB_UI_EDIT_CURRENT
} fsbb_ui_edit_target_t;

typedef enum _tag_fsbb_ui_step_mode_t
{
    FSBB_UI_STEP_FINE = 0,
    FSBB_UI_STEP_COARSE
} fsbb_ui_step_mode_t;

typedef enum _tag_fsbb_ui_input_error_t
{
    FSBB_UI_INPUT_OK = 0,
    FSBB_UI_INPUT_FORMAT,
    FSBB_UI_INPUT_RANGE
} fsbb_ui_input_error_t;

typedef struct _tag_fsbb_ui_input_t
{
    fast_gt active;
    fsbb_ui_edit_target_t target;
    char buffer[FSBB_UI_INPUT_BUFFER_SIZE];
    uint16_t length;
    uint16_t idle_ticks;
    fsbb_ui_input_error_t error;
} fsbb_ui_input_t;

static ht16k33_dev_t fsbb_ui_device;
static volatile fast_gt fsbb_ui_ready = 0;
static volatile fast_gt fsbb_oled_ready = 0;
static volatile fast_gt fsbb_ui_started = 0;
static fsbb_ui_edit_target_t fsbb_ui_selected_target = FSBB_UI_EDIT_VOLTAGE;
static fsbb_ui_step_mode_t fsbb_ui_step_mode = FSBB_UI_STEP_FINE;
static fsbb_ui_input_t fsbb_ui_input = {
    0, FSBB_UI_EDIT_VOLTAGE, {0}, 0U, 0U, FSBB_UI_INPUT_OK};

static float fsbb_ui_vout_filtered = 0.0f;
static float fsbb_ui_iout_filtered = 0.0f;
static float fsbb_ui_vout_average_buffer[FSBB_UI_VOUT_DISPLAY_AVERAGE_SAMPLES];
static float fsbb_ui_iout_average_buffer[FSBB_UI_IOUT_DISPLAY_AVERAGE_SAMPLES];
static float fsbb_ui_vout_average_sum = 0.0f;
static float fsbb_ui_iout_average_sum = 0.0f;
static uint16_t fsbb_ui_vout_average_index = 0U;
static uint16_t fsbb_ui_iout_average_index = 0U;
static fast_gt fsbb_ui_average_primed = 0;

// Common-cathode 7-segment codes, identical to the verified reference project.
static const data_gt fsbb_ui_digit_lut[16] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};

#define FSBB_UI_SEG_U     ((data_gt)0x3E)
#define FSBB_UI_SEG_R     ((data_gt)0x50)
#define FSBB_UI_SEG_N     ((data_gt)0x54)
#define FSBB_UI_SEG_DASH  ((data_gt)0x40)
#define FSBB_UI_SEG_DOT   ((data_gt)0x80)
#define FSBB_UI_SEG_BLANK ((data_gt)0x00)

//=================================================================================================
// SSD1306/SH1106 OLED support migrated from the verified Iris panel project

static const data_gt fsbb_oled_digit_font[10][6] = {
    {0x00, 0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x00, 0x42, 0x61, 0x51, 0x49, 0x46},
    {0x00, 0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x00, 0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x00, 0x27, 0x45, 0x45, 0x45, 0x39},
    {0x00, 0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x00, 0x01, 0x71, 0x09, 0x05, 0x03},
    {0x00, 0x36, 0x49, 0x49, 0x49, 0x36},
    {0x00, 0x06, 0x49, 0x49, 0x29, 0x1E}};

static const data_gt fsbb_oled_upper_font[26][6] = {
    {0x00, 0x7C, 0x12, 0x11, 0x12, 0x7C},
    {0x00, 0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x00, 0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x00, 0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x00, 0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x00, 0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x00, 0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x00, 0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x00, 0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x00, 0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x00, 0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x00, 0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x00, 0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x00, 0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x00, 0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x00, 0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x00, 0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x00, 0x46, 0x49, 0x49, 0x49, 0x31},
    {0x00, 0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x00, 0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x00, 0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x00, 0x3F, 0x40, 0x38, 0x40, 0x3F},
    {0x00, 0x63, 0x14, 0x08, 0x14, 0x63},
    {0x00, 0x07, 0x08, 0x70, 0x08, 0x07},
    {0x00, 0x61, 0x51, 0x49, 0x45, 0x43}};

static const data_gt fsbb_oled_blank_glyph[6] = {0, 0, 0, 0, 0, 0};
static const data_gt fsbb_oled_colon_glyph[6] = {0, 0, 0x36, 0x36, 0, 0};
static const data_gt fsbb_oled_dot_glyph[6] = {0, 0, 0x60, 0x60, 0, 0};
static const data_gt fsbb_oled_dash_glyph[6] = {0, 0x08, 0x08, 0x08, 0x08, 0x08};
static const data_gt fsbb_oled_equal_glyph[6] = {0, 0x14, 0x14, 0x14, 0x14, 0x14};
static const data_gt fsbb_oled_star_glyph[6] = {0, 0x14, 0x08, 0x3E, 0x08, 0x14};

static const data_gt* fsbb_oled_get_glyph(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
        return fsbb_oled_digit_font[(uint16_t)(ch - '0')];
    if ((ch >= 'A') && (ch <= 'Z'))
        return fsbb_oled_upper_font[(uint16_t)(ch - 'A')];

    switch (ch)
    {
    case ':': return fsbb_oled_colon_glyph;
    case '.': return fsbb_oled_dot_glyph;
    case '-': return fsbb_oled_dash_glyph;
    case '=': return fsbb_oled_equal_glyph;
    case '*': return fsbb_oled_star_glyph;
    default: return fsbb_oled_blank_glyph;
    }
}

static ec_gt fsbb_oled_write_command(data_gt command)
{
    return gmp_hal_iic_write_reg(
        IRIS_IIC_BASE,
        FSBB_OLED_I2C_ADDRESS,
        0x00U,
        1U,
        command,
        1U,
        FSBB_OLED_I2C_TIMEOUT_MS);
}

static ec_gt fsbb_oled_set_line_position(uint16_t page)
{
    data_gt position[4];

    position[0] = 0x00U;
    position[1] = (data_gt)(0xB0U + page);
    position[2] = 0x10U;
    position[3] = 0x02U;

    return gmp_hal_iic_write_mem(
        IRIS_IIC_BASE,
        FSBB_OLED_I2C_ADDRESS,
        0U,
        0U,
        position,
        4U,
        FSBB_OLED_I2C_TIMEOUT_MS);
}

static ec_gt fsbb_oled_clear(void)
{
    static data_gt blank_line[FSBB_OLED_WIDTH + 1U];
    uint16_t page;
    uint16_t index;
    ec_gt result;

    blank_line[0] = 0x40U;
    for (index = 1U; index <= FSBB_OLED_WIDTH; ++index)
        blank_line[index] = 0x00U;

    for (page = 0U; page < FSBB_OLED_PAGE_COUNT; ++page)
    {
        result = fsbb_oled_set_line_position(page);
        if (result != GMP_EC_OK)
            return result;

        result = gmp_hal_iic_write_mem(
            IRIS_IIC_BASE,
            FSBB_OLED_I2C_ADDRESS,
            0U,
            0U,
            blank_line,
            FSBB_OLED_WIDTH + 1U,
            FSBB_OLED_I2C_TIMEOUT_MS);
        if (result != GMP_EC_OK)
            return result;
    }

    return GMP_EC_OK;
}

static ec_gt fsbb_oled_init(void)
{
    static const data_gt init_commands[] = {
        0xAE, 0x02, 0x10, 0x40,
        0x81, 0xCF, 0xA1, 0xC8,
        0xA6, 0xA8, 0x3F, 0xD3,
        0x00, 0xD5, 0x80, 0xD9,
        0xF1, 0xDA, 0x12, 0xDB,
        0x40, 0x20, 0x02, 0x8D,
        0x14, 0xA4, 0xA6, 0xAF};
    uint16_t index;
    ec_gt result;

    for (index = 0U; index < (sizeof(init_commands) / sizeof(init_commands[0])); ++index)
    {
        result = fsbb_oled_write_command(init_commands[index]);
        if (result != GMP_EC_OK)
            return result;

        if ((index & 3U) == 3U)
            DEVICE_DELAY_US(200U);
    }

    return fsbb_oled_clear();
}

static ec_gt fsbb_oled_write_line(uint16_t line_index, const char* text)
{
    static data_gt payload[1U + FSBB_OLED_LINE_CHARS * 6U];
    uint16_t character;
    uint16_t column;
    const data_gt* glyph;
    char ch;
    ec_gt result;

    if ((line_index >= 4U) || (text == NULL))
        return GMP_EC_GENERAL_ERROR;

    payload[0] = 0x40U;
    for (character = 0U; character < FSBB_OLED_LINE_CHARS; ++character)
    {
        ch = text[character];
        if (ch == '\0')
            ch = ' ';

        glyph = fsbb_oled_get_glyph(ch);
        for (column = 0U; column < 6U; ++column)
            payload[1U + character * 6U + column] = glyph[column];
    }

    result = fsbb_oled_set_line_position(line_index * 2U);
    if (result != GMP_EC_OK)
        return result;

    return gmp_hal_iic_write_mem(
        IRIS_IIC_BASE,
        FSBB_OLED_I2C_ADDRESS,
        0U,
        0U,
        payload,
        1U + FSBB_OLED_LINE_CHARS * 6U,
        FSBB_OLED_I2C_TIMEOUT_MS);
}

static void fsbb_oled_pad_line(char* line)
{
    uint16_t index = 0U;

    while ((index < FSBB_OLED_LINE_CHARS) && (line[index] != '\0'))
        ++index;
    while (index < FSBB_OLED_LINE_CHARS)
        line[index++] = ' ';
    line[FSBB_OLED_LINE_CHARS] = '\0';
}

static void fsbb_oled_build_line(uint16_t line_index, char line[FSBB_OLED_LINE_CHARS + 1U])
{
    float display_vout = fsbb_ui_vout_filtered;
    float display_iout = fsbb_ui_iout_filtered;
    uint16_t vset_tenths = (uint16_t)(ctrl2float(g_v_out_ref_user) * CTRL_VOLTAGE_BASE * 10.0f + 0.5f);
    uint16_t iset_hundredths = (uint16_t)(ctrl2float(g_i_limit_user) * CTRL_CURRENT_BASE * 100.0f + 0.5f);
    uint16_t vout_tenths;
    uint16_t iout_hundredths;

    if (display_vout < 0.0f)
        display_vout = 0.0f;
    if (display_vout > 99.9f)
        display_vout = 99.9f;
    if (display_iout < 0.0f)
        display_iout = 0.0f;
    if (display_iout > 9.99f)
        display_iout = 9.99f;

    vout_tenths = (uint16_t)(display_vout * 10.0f + 0.5f);
    iout_hundredths = (uint16_t)(display_iout * 100.0f + 0.5f);

    if (g_fsbb_faults != FSBB_FAULT_NONE)
    {
        switch (line_index)
        {
        case 0U: sprintf(line, "*** FSBB FAULT ***"); break;
        case 1U: sprintf(line, "MASK:0X%04X", g_fsbb_faults); break;
        case 2U: sprintf(line, "OUTPUT DISABLED"); break;
        default: sprintf(line, "SW14=FAULT RESET"); break;
        }
    }
    else if (fsbb_ui_input.active != 0)
    {
        switch (line_index)
        {
        case 0U:
            sprintf(line, "INPUT:%s", (fsbb_ui_input.target == FSBB_UI_EDIT_VOLTAGE) ? "VOLTAGE" : "CURRENT");
            break;
        case 1U: sprintf(line, "VALUE:%s", fsbb_ui_input.buffer); break;
        case 2U:
            sprintf(line, "%s", (fsbb_ui_input.target == FSBB_UI_EDIT_VOLTAGE) ?
                "RANGE:10.0-50.0V" : "RANGE:1.00-5.00A");
            break;
        default:
            sprintf(line, "%s", (fsbb_ui_input.error == FSBB_UI_INPUT_OK) ?
                "SW2=OK SW10=CLEAR" : "ERROR:CHECK VALUE");
            break;
        }
    }
    else
    {
        switch (line_index)
        {
        case 0U:
            sprintf(line, "FSBB CV     OUT:%s", (g_fsbb_output_enabled != 0) ? "ON" : "OFF");
            break;
        case 1U:
            sprintf(line, "SET %02u.%uV %u.%02uA",
                vset_tenths / 10U, vset_tenths % 10U,
                iset_hundredths / 100U, iset_hundredths % 100U);
            break;
        case 2U:
            sprintf(line, "OUT %02u.%uV %u.%02uA",
                vout_tenths / 10U, vout_tenths % 10U,
                iout_hundredths / 100U, iout_hundredths % 100U);
            break;
        default:
            sprintf(line, "EDIT:%c STEP:%s",
                (fsbb_ui_selected_target == FSBB_UI_EDIT_VOLTAGE) ? 'V' : 'I',
                (fsbb_ui_step_mode == FSBB_UI_STEP_FINE) ? "FINE" : "COARSE");
            break;
        }
    }

    fsbb_oled_pad_line(line);
}

//=================================================================================================
// Display helpers

static void fsbb_ui_write_display(const data_gt segments[8])
{
    uint16_t index;

    for (index = 0U; index < 8U; ++index)
        fsbb_ui_device.display_ram[index * 2U] = segments[index];

    fsbb_ui_device.is_dirty = 1;
}

static data_gt fsbb_ui_hex_segment(uint16_t value)
{
    return fsbb_ui_digit_lut[value & 0x0FU];
}

static float fsbb_ui_read_output_current_a(void)
{
#if defined FSBB_ENABLE_IOUT_SAMPLE
    return ctrl2float(adc_i_load.control_port.value) * CTRL_CURRENT_BASE;
#else
    // Never label inductor current as output current. A disabled IOUT path is
    // an unavailable measurement, so the display remains at zero.
    return 0.0f;
#endif
}

static float fsbb_ui_apply_display_zero(float value, float threshold)
{
    if ((value > -threshold) && (value < threshold))
        return 0.0f;

    return value;
}

static void fsbb_ui_render_voltage(data_gt* segments, float value)
{
    uint16_t scaled;

    if (value < 0.0f)
        value = 0.0f;
    if (value > 99.9f)
        value = 99.9f;

    scaled = (uint16_t)(value * 10.0f + 0.5f);
    segments[0] = (scaled >= 100U) ? fsbb_ui_digit_lut[(scaled / 100U) % 10U] : FSBB_UI_SEG_BLANK;
    segments[1] = fsbb_ui_digit_lut[(scaled / 10U) % 10U] | FSBB_UI_SEG_DOT;
    segments[2] = fsbb_ui_digit_lut[scaled % 10U];
}

static void fsbb_ui_render_current(data_gt* segments, float value)
{
    uint16_t scaled;

    if (value < 0.0f)
        value = 0.0f;
    if (value > 9.99f)
        value = 9.99f;

    scaled = (uint16_t)(value * 100.0f + 0.5f);
    segments[0] = fsbb_ui_digit_lut[(scaled / 100U) % 10U] | FSBB_UI_SEG_DOT;
    segments[1] = fsbb_ui_digit_lut[(scaled / 10U) % 10U];
    segments[2] = fsbb_ui_digit_lut[scaled % 10U];
}

static void fsbb_ui_render_fault(data_gt segments[8])
{
    uint16_t faults = g_fsbb_faults;

    segments[0] = fsbb_ui_digit_lut[15U];
    segments[1] = fsbb_ui_hex_segment(faults >> 12U);
    segments[2] = fsbb_ui_hex_segment(faults >> 8U);
    segments[3] = fsbb_ui_hex_segment(faults >> 4U);
    segments[4] = fsbb_ui_hex_segment(faults);
    segments[5] = FSBB_UI_SEG_DASH;
    segments[6] = fsbb_ui_digit_lut[14U];
    segments[7] = FSBB_UI_SEG_R;
}

static void fsbb_ui_render_entry(data_gt segments[8])
{
    uint16_t source;
    uint16_t destination = 1U;

    segments[0] = (fsbb_ui_input.target == FSBB_UI_EDIT_VOLTAGE) ?
        FSBB_UI_SEG_U : fsbb_ui_digit_lut[10U];

    if (fsbb_ui_input.error != FSBB_UI_INPUT_OK)
    {
        segments[1] = fsbb_ui_digit_lut[14U];
        segments[2] = FSBB_UI_SEG_R;
        segments[3] = FSBB_UI_SEG_R;
        return;
    }

    for (source = 0U;
         (source < fsbb_ui_input.length) && (destination < 8U);
         ++source)
    {
        char ch = fsbb_ui_input.buffer[source];

        if (ch == '.')
        {
            if (destination > 1U)
                segments[destination - 1U] |= FSBB_UI_SEG_DOT;
            continue;
        }

        if ((ch >= '0') && (ch <= '9'))
            segments[destination++] = fsbb_ui_digit_lut[(uint16_t)(ch - '0')];
    }
}

static void fsbb_ui_compose_display(void)
{
    data_gt segments[8] = {
        FSBB_UI_SEG_BLANK, FSBB_UI_SEG_BLANK, FSBB_UI_SEG_BLANK, FSBB_UI_SEG_BLANK,
        FSBB_UI_SEG_BLANK, FSBB_UI_SEG_BLANK, FSBB_UI_SEG_BLANK, FSBB_UI_SEG_BLANK};
    float setpoint;

    if (fsbb_ui_input.active != 0)
    {
        fsbb_ui_render_entry(segments);
        fsbb_ui_write_display(segments);
        return;
    }

    if (g_fsbb_faults != FSBB_FAULT_NONE)
    {
        fsbb_ui_render_fault(segments);
        fsbb_ui_write_display(segments);
        return;
    }

    if (fsbb_ui_selected_target == FSBB_UI_EDIT_VOLTAGE)
    {
        setpoint = ctrl2float(g_v_out_ref_user) * CTRL_VOLTAGE_BASE;
        segments[0] = FSBB_UI_SEG_U;
        fsbb_ui_render_voltage(&segments[1], setpoint);
        fsbb_ui_render_voltage(&segments[5], fsbb_ui_vout_filtered);
    }
    else
    {
        setpoint = ctrl2float(g_i_limit_user) * CTRL_CURRENT_BASE;
        segments[0] = fsbb_ui_digit_lut[10U];
        fsbb_ui_render_current(&segments[1], setpoint);
        fsbb_ui_render_current(&segments[5], fsbb_ui_iout_filtered);
    }

    // Centre digit is also a compact output-state indicator: 'n' means on,
    // '-' means disabled. The three left digits are setpoint; right are actual.
    segments[4] = (g_fsbb_output_enabled != 0) ? FSBB_UI_SEG_N : FSBB_UI_SEG_DASH;
    fsbb_ui_write_display(segments);
}

//=================================================================================================
// Numeric input helpers

static void fsbb_ui_reset_input(void)
{
    uint16_t index;

    fsbb_ui_input.active = 0;
    fsbb_ui_input.length = 0U;
    fsbb_ui_input.idle_ticks = 0U;
    fsbb_ui_input.error = FSBB_UI_INPUT_OK;

    for (index = 0U; index < FSBB_UI_INPUT_BUFFER_SIZE; ++index)
        fsbb_ui_input.buffer[index] = '\0';
}

static void fsbb_ui_start_input(void)
{
    fsbb_ui_reset_input();
    fsbb_ui_input.active = 1;
    fsbb_ui_input.target = fsbb_ui_selected_target;
}

static void fsbb_ui_append_digit(uint16_t digit)
{
    if (digit > 9U)
        return;

    if (fsbb_ui_input.active == 0)
        fsbb_ui_start_input();

    if (fsbb_ui_input.length >= (FSBB_UI_INPUT_BUFFER_SIZE - 1U))
        return;

    fsbb_ui_input.buffer[fsbb_ui_input.length++] = (char)('0' + digit);
    fsbb_ui_input.buffer[fsbb_ui_input.length] = '\0';
    fsbb_ui_input.idle_ticks = 0U;
    fsbb_ui_input.error = FSBB_UI_INPUT_OK;
}

static void fsbb_ui_append_decimal(void)
{
    uint16_t index;

    if (fsbb_ui_input.active == 0)
        fsbb_ui_start_input();

    for (index = 0U; index < fsbb_ui_input.length; ++index)
    {
        if (fsbb_ui_input.buffer[index] == '.')
        {
            fsbb_ui_input.error = FSBB_UI_INPUT_FORMAT;
            return;
        }
    }

    if (fsbb_ui_input.length == 0U)
        fsbb_ui_append_digit(0U);

    if (fsbb_ui_input.length < (FSBB_UI_INPUT_BUFFER_SIZE - 1U))
    {
        fsbb_ui_input.buffer[fsbb_ui_input.length++] = '.';
        fsbb_ui_input.buffer[fsbb_ui_input.length] = '\0';
        fsbb_ui_input.idle_ticks = 0U;
        fsbb_ui_input.error = FSBB_UI_INPUT_OK;
    }
}

static void fsbb_ui_backspace(void)
{
    if ((fsbb_ui_input.active == 0) || (fsbb_ui_input.length == 0U))
        return;

    fsbb_ui_input.buffer[--fsbb_ui_input.length] = '\0';
    fsbb_ui_input.idle_ticks = 0U;
    fsbb_ui_input.error = FSBB_UI_INPUT_OK;
}

static fast_gt fsbb_ui_parse_hundredths(uint16_t* result)
{
    uint16_t index;
    uint16_t whole = 0U;
    uint16_t fraction = 0U;
    uint16_t fraction_digits = 0U;
    uint16_t digit_count = 0U;
    fast_gt decimal_seen = 0;

    if (fsbb_ui_input.length == 0U)
        return 0;

    for (index = 0U; index < fsbb_ui_input.length; ++index)
    {
        char ch = fsbb_ui_input.buffer[index];

        if (ch == '.')
        {
            if (decimal_seen != 0)
                return 0;
            decimal_seen = 1;
            continue;
        }

        if ((ch < '0') || (ch > '9'))
            return 0;

        digit_count++;
        if (decimal_seen == 0)
        {
            whole = (uint16_t)(whole * 10U + (uint16_t)(ch - '0'));
            if (whole > 99U)
                return 0;
        }
        else
        {
            if (++fraction_digits > 2U)
                return 0;
            fraction = (uint16_t)(fraction * 10U + (uint16_t)(ch - '0'));
        }
    }

    if (digit_count == 0U)
        return 0;

    if (fraction_digits == 0U)
        fraction = 0U;
    else if (fraction_digits == 1U)
        fraction *= 10U;

    *result = (uint16_t)(whole * 100U + fraction);
    return 1;
}

static void fsbb_ui_commit_input(void)
{
    uint16_t hundredths;
    float value;

    if (fsbb_ui_input.active == 0)
        return;

    if (fsbb_ui_parse_hundredths(&hundredths) == 0)
    {
        fsbb_ui_input.error = FSBB_UI_INPUT_FORMAT;
        fsbb_ui_input.idle_ticks = 0U;
        return;
    }

    value = (float)hundredths / 100.0f;
    if (fsbb_ui_input.target == FSBB_UI_EDIT_VOLTAGE)
    {
        if ((value < FSBB_UI_VSET_MIN_V) || (value > FSBB_UI_VSET_MAX_V))
        {
            fsbb_ui_input.error = FSBB_UI_INPUT_RANGE;
            fsbb_ui_input.idle_ticks = 0U;
            return;
        }

        g_v_out_ref_user = float2ctrl(value / CTRL_VOLTAGE_BASE);
        gmp_base_print("Vset accepted: %u.%02u V\r\n", hundredths / 100U, hundredths % 100U);
    }
    else
    {
        if ((value < FSBB_UI_ISET_MIN_A) || (value > FSBB_UI_ISET_MAX_A))
        {
            fsbb_ui_input.error = FSBB_UI_INPUT_RANGE;
            fsbb_ui_input.idle_ticks = 0U;
            return;
        }

        // The present BUILD_LEVEL 3 controller consumes this as the inductor
        // current limit. It will be remapped to g_i_out_ref_user with the CC loop.
        g_i_limit_user = float2ctrl(value / CTRL_CURRENT_BASE);
        gmp_base_print("Current limit accepted: %u.%02u A\r\n", hundredths / 100U, hundredths % 100U);
    }

    fsbb_ui_reset_input();
}

static fast_gt fsbb_ui_key_to_digit(fast_gt key_id, uint16_t* digit)
{
    switch ((uint16_t)key_id)
    {
    case FSBB_UI_KEY_DIGIT_0_ID: *digit = 0U; return 1;
    case FSBB_UI_KEY_DIGIT_1_ID: *digit = 1U; return 1;
    case FSBB_UI_KEY_DIGIT_2_ID: *digit = 2U; return 1;
    case FSBB_UI_KEY_DIGIT_3_ID: *digit = 3U; return 1;
    case FSBB_UI_KEY_DIGIT_4_ID: *digit = 4U; return 1;
    case FSBB_UI_KEY_DIGIT_5_ID: *digit = 5U; return 1;
    case FSBB_UI_KEY_DIGIT_6_ID: *digit = 6U; return 1;
    case FSBB_UI_KEY_DIGIT_7_ID: *digit = 7U; return 1;
    case FSBB_UI_KEY_DIGIT_8_ID: *digit = 8U; return 1;
    case FSBB_UI_KEY_DIGIT_9_ID: *digit = 9U; return 1;
    default: *digit = 0U; return 0;
    }
}

static void fsbb_ui_handle_output_key(void)
{
    fsbb_ui_reset_input();

    if ((g_fsbb_faults != FSBB_FAULT_NONE) ||
        (cia402_sm.current_state == CIA402_SM_FAULT_REACTION) ||
        (cia402_sm.current_state == CIA402_SM_FAULT))
    {
        if (ctl_request_fault_reset())
            gmp_base_print(TEXT_STRING("Fault reset accepted; press output again after recovery.\r\n"));
        else
            gmp_base_print(TEXT_STRING("Fault reset blocked; remove the active fault first.\r\n"));
        return;
    }

    if ((g_fsbb_output_enabled != 0) ||
        (cia402_sm.current_cmd == CIA402_CMD_ENABLE_OPERATION))
    {
        cia402_send_cmd(&cia402_sm, CIA402_CMD_DISABLE_VOLTAGE);
        gmp_base_print(TEXT_STRING("Output-off requested.\r\n"));
    }
    else
    {
        cia402_send_cmd(&cia402_sm, CIA402_CMD_ENABLE_OPERATION);
        gmp_base_print(TEXT_STRING("Output-on requested.\r\n"));
    }
}

//=================================================================================================
// Public startup and scheduler tasks

ec_gt fsbb_ui_start(void)
{
    ht16k33_init_t init_cfg = {
        .brightness = 15,
        .blink_rate = 0,
        .int_enable = 0,
        .int_act_high = 0};
    ec_gt ht16k33_result;
    ec_gt oled_result;

    fsbb_ui_ready = 0;
    fsbb_oled_ready = 0;
    fsbb_ui_started = 0;

    oled_result = fsbb_oled_init();
    if (oled_result == GMP_EC_OK)
        fsbb_oled_ready = 1;
    else
        gmp_base_print(TEXT_STRING("OLED init failed; OLED display disabled.\r\n"));

    ht16k33_result = ht16k33_init(
        &fsbb_ui_device,
        IRIS_IIC_BASE,
        HT16K33_DEFAULT_DEV_ADDR,
        &init_cfg);

    if (ht16k33_result == GMP_EC_OK)
        fsbb_ui_ready = 1;
    else
        gmp_base_print(TEXT_STRING("HT16K33 init failed; keypad/7-segment disabled.\r\n"));

    if ((fsbb_ui_ready != 0) || (fsbb_oled_ready != 0))
    {
        fsbb_ui_average_primed = 0;
        fsbb_ui_reset_input();
        fsbb_ui_started = 1;
        if (fsbb_ui_ready != 0)
            fsbb_ui_compose_display();
        return GMP_EC_OK;
    }

    return oled_result;
}

gmp_task_status_t tsk_fsbb_ui_key(gmp_task_t* tsk)
{
    static fast_gt key_armed = 1;
    static uint16_t key_quiet_ticks = 0U;
    fast_gt key_id = 0;
    uint16_t digit = 0U;
    ec_gt result;

    GMP_UNUSED_VAR(tsk);

    if (fsbb_ui_ready == 0)
        return GMP_TASK_DONE;

    result = ht16k33_read_keys(&fsbb_ui_device, &key_id);
    if (result != GMP_EC_OK)
    {
        fsbb_ui_ready = 0;
        gmp_base_print(TEXT_STRING("HT16K33 key scan failed; UI disabled.\r\n"));
        return GMP_TASK_DONE;
    }

    if (key_id == 0)
    {
        if (key_armed == 0)
        {
            if (key_quiet_ticks < FSBB_UI_KEY_RELEASE_TICKS)
                key_quiet_ticks++;
            if (key_quiet_ticks >= FSBB_UI_KEY_RELEASE_TICKS)
            {
                key_armed = 1;
                key_quiet_ticks = 0U;
            }
        }

        if (fsbb_ui_input.active != 0)
        {
            if (fsbb_ui_input.idle_ticks < FSBB_UI_INPUT_TIMEOUT_TICKS)
                fsbb_ui_input.idle_ticks++;
            if (fsbb_ui_input.idle_ticks >= FSBB_UI_INPUT_TIMEOUT_TICKS)
                fsbb_ui_reset_input();
        }
        return GMP_TASK_DONE;
    }

    fsbb_ui_input.idle_ticks = 0U;
    if (key_armed == 0)
    {
        key_quiet_ticks = 0U;
        return GMP_TASK_DONE;
    }

    key_armed = 0;
    key_quiet_ticks = 0U;

    // Keep the verified panel project's one-shot key log. This is especially
    // useful on hardware to confirm that SW14 is decoded as key ID 16.
    gmp_base_print(TEXT_STRING("Key ID: %d\r\n"), key_id);

    if (fsbb_ui_key_to_digit(key_id, &digit) != 0)
    {
        fsbb_ui_append_digit(digit);
        return GMP_TASK_DONE;
    }

    switch ((uint16_t)key_id)
    {
    case FSBB_UI_KEY_DECIMAL_ID:
        fsbb_ui_append_decimal();
        break;
    case FSBB_UI_KEY_CONFIRM_ID:
        fsbb_ui_commit_input();
        break;
    case FSBB_UI_KEY_EDIT_TOGGLE_ID:
        fsbb_ui_reset_input();
        fsbb_ui_selected_target =
            (fsbb_ui_selected_target == FSBB_UI_EDIT_VOLTAGE) ?
                FSBB_UI_EDIT_CURRENT : FSBB_UI_EDIT_VOLTAGE;
        break;
    case FSBB_UI_KEY_MODE_ID:
        fsbb_ui_reset_input();
        gmp_base_print(TEXT_STRING("Mode key reserved until AUTO/CV/CC control is connected.\r\n"));
        break;
    case FSBB_UI_KEY_OUTPUT_ID:
        fsbb_ui_handle_output_key();
        break;
    case FSBB_UI_KEY_CLEAR_ID:
        fsbb_ui_reset_input();
        break;
    case FSBB_UI_KEY_BACKSPACE_ID:
        fsbb_ui_backspace();
        break;
    default:
        break;
    }

    return GMP_TASK_DONE;
}

//=================================================================================================
// Rotary encoder

static void fsbb_ui_toggle_step_mode(void)
{
    fsbb_ui_step_mode =
        (fsbb_ui_step_mode == FSBB_UI_STEP_FINE) ?
            FSBB_UI_STEP_COARSE : FSBB_UI_STEP_FINE;

    fsbb_ui_reset_input();
    gmp_base_print(
        "Encoder step: %s\r\n",
        (fsbb_ui_step_mode == FSBB_UI_STEP_FINE) ? "fine" : "coarse");
}

static void fsbb_ui_adjust_selected_setting(int32_t step_count)
{
    int32_t setting_hundredths;
    int32_t unit_step;
    int32_t minimum;
    int32_t maximum;

    if (step_count == 0L)
        return;

    // A knob movement takes ownership of the setting and cancels any numeric
    // entry that has not yet been confirmed.
    fsbb_ui_reset_input();

    if (fsbb_ui_selected_target == FSBB_UI_EDIT_VOLTAGE)
    {
        setting_hundredths = (int32_t)(
            ctrl2float(g_v_out_ref_user) * CTRL_VOLTAGE_BASE * 100.0f + 0.5f);
        unit_step = (fsbb_ui_step_mode == FSBB_UI_STEP_FINE) ?
            FSBB_UI_ENCODER_V_FINE_STEP_CV : FSBB_UI_ENCODER_V_COARSE_STEP_CV;
        minimum = (int32_t)(FSBB_UI_VSET_MIN_V * 100.0f + 0.5f);
        maximum = (int32_t)(FSBB_UI_VSET_MAX_V * 100.0f + 0.5f);

        setting_hundredths += step_count * unit_step;
        if (setting_hundredths < minimum)
            setting_hundredths = minimum;
        if (setting_hundredths > maximum)
            setting_hundredths = maximum;

        g_v_out_ref_user = float2ctrl(
            ((float)setting_hundredths / 100.0f) / CTRL_VOLTAGE_BASE);
    }
    else
    {
        setting_hundredths = (int32_t)(
            ctrl2float(g_i_limit_user) * CTRL_CURRENT_BASE * 100.0f + 0.5f);
        unit_step = (fsbb_ui_step_mode == FSBB_UI_STEP_FINE) ?
            FSBB_UI_ENCODER_I_FINE_STEP_CA : FSBB_UI_ENCODER_I_COARSE_STEP_CA;
        minimum = (int32_t)(FSBB_UI_ISET_MIN_A * 100.0f + 0.5f);
        maximum = (int32_t)(FSBB_UI_ISET_MAX_A * 100.0f + 0.5f);

        setting_hundredths += step_count * unit_step;
        if (setting_hundredths < minimum)
            setting_hundredths = minimum;
        if (setting_hundredths > maximum)
            setting_hundredths = maximum;

        g_i_limit_user = float2ctrl(
            ((float)setting_hundredths / 100.0f) / CTRL_CURRENT_BASE);
    }
}

gmp_task_status_t tsk_fsbb_ui_encoder(gmp_task_t* tsk)
{
    static fast_gt initialized = 0;
    static fast_gt button_armed = 1;
    static uint16_t button_release_ticks = 0U;
    static uint32_t previous_position = 0U;
    static int32_t count_accumulator = 0L;
    uint32_t current_position;
    int32_t position_delta;
    int32_t step_count = 0L;
    fast_gt button_pressed;

    GMP_UNUSED_VAR(tsk);

    if (fsbb_ui_started == 0)
        return GMP_TASK_DONE;

    if (initialized == 0)
    {
        EQEP_setPositionCounterConfig(
            FSBB_UI_ENCODER_BASE,
            EQEP_POSITION_RESET_MAX_POS,
            (uint32_t)FSBB_UI_ENCODER_MAX_POSITION);
        EQEP_setPosition(FSBB_UI_ENCODER_BASE, 0U);

        previous_position = 0U;
        count_accumulator = 0L;
        initialized = 1;
        return GMP_TASK_DONE;
    }

    button_pressed =
        (GPIO_readPin(FSBB_UI_ENCODER_BUTTON_PORT) ==
         FSBB_UI_ENCODER_BUTTON_ACTIVE_LEVEL);

    if (button_pressed != 0)
    {
        button_release_ticks = 0U;
        if (button_armed != 0)
        {
            button_armed = 0;
            fsbb_ui_toggle_step_mode();
        }
    }
    else if (button_armed == 0)
    {
        if (button_release_ticks < FSBB_UI_ENCODER_BUTTON_RELEASE_TICKS)
            button_release_ticks++;
        if (button_release_ticks >= FSBB_UI_ENCODER_BUTTON_RELEASE_TICKS)
        {
            button_armed = 1;
            button_release_ticks = 0U;
        }
    }

    current_position = EQEP_getPosition(FSBB_UI_ENCODER_BASE);
    position_delta = (int32_t)current_position - (int32_t)previous_position;
    previous_position = current_position;

    if (position_delta > (int32_t)(FSBB_UI_ENCODER_MAX_POSITION / 2UL))
        position_delta -= (int32_t)(FSBB_UI_ENCODER_MAX_POSITION + 1UL);
    else if (position_delta < -((int32_t)(FSBB_UI_ENCODER_MAX_POSITION / 2UL)))
        position_delta += (int32_t)(FSBB_UI_ENCODER_MAX_POSITION + 1UL);

    count_accumulator += position_delta;
    while (count_accumulator >= FSBB_UI_ENCODER_COUNTS_PER_STEP)
    {
        step_count++;
        count_accumulator -= FSBB_UI_ENCODER_COUNTS_PER_STEP;
    }
    while (count_accumulator <= -FSBB_UI_ENCODER_COUNTS_PER_STEP)
    {
        step_count--;
        count_accumulator += FSBB_UI_ENCODER_COUNTS_PER_STEP;
    }

#if FSBB_UI_ENCODER_REVERSE
    step_count = -step_count;
#endif

    fsbb_ui_adjust_selected_setting(step_count);
    return GMP_TASK_DONE;
}

gmp_task_status_t tsk_fsbb_ui_display(gmp_task_t* tsk)
{
    static uint16_t oled_line_index = 0U;
    float vout;
    float iout;
    char oled_line[FSBB_OLED_LINE_CHARS + 1U];
    ec_gt result;

    GMP_UNUSED_VAR(tsk);

    if ((fsbb_ui_ready == 0) && (fsbb_oled_ready == 0))
        return GMP_TASK_DONE;

    if (flag_enable_adc_calibrator != 0)
    {
        // Current channels still contain their nominal mid-supply offset
        // while startup calibration is running. Do not display that transient
        // as a real load current.
        vout = 0.0f;
        iout = 0.0f;
    }
    else
    {
        vout = ctrl2float(adc_v_out.control_port.value) * CTRL_VOLTAGE_BASE;
        iout = fsbb_ui_read_output_current_a();

        // Display-only zero suppression. The ADC values used by control and
        // protection remain untouched and available for calibration/debug.
        vout = fsbb_ui_apply_display_zero(vout, FSBB_UI_VOUT_DISPLAY_ZERO_V);
        iout = fsbb_ui_apply_display_zero(iout, FSBB_UI_IOUT_DISPLAY_ZERO_A);
    }

    if (fsbb_ui_average_primed == 0)
    {
        uint16_t index;

        for (index = 0U; index < FSBB_UI_VOUT_DISPLAY_AVERAGE_SAMPLES; ++index)
            fsbb_ui_vout_average_buffer[index] = vout;

        for (index = 0U; index < FSBB_UI_IOUT_DISPLAY_AVERAGE_SAMPLES; ++index)
            fsbb_ui_iout_average_buffer[index] = iout;

        fsbb_ui_vout_average_sum = vout * FSBB_UI_VOUT_DISPLAY_AVERAGE_SAMPLES;
        fsbb_ui_iout_average_sum = iout * FSBB_UI_IOUT_DISPLAY_AVERAGE_SAMPLES;
        fsbb_ui_vout_average_index = 0U;
        fsbb_ui_iout_average_index = 0U;
        fsbb_ui_vout_filtered = vout;
        fsbb_ui_iout_filtered = iout;
        fsbb_ui_average_primed = 1;
    }
    else
    {
        fsbb_ui_vout_average_sum -= fsbb_ui_vout_average_buffer[fsbb_ui_vout_average_index];
        fsbb_ui_vout_average_buffer[fsbb_ui_vout_average_index] = vout;
        fsbb_ui_vout_average_sum += vout;

        fsbb_ui_iout_average_sum -= fsbb_ui_iout_average_buffer[fsbb_ui_iout_average_index];
        fsbb_ui_iout_average_buffer[fsbb_ui_iout_average_index] = iout;
        fsbb_ui_iout_average_sum += iout;

        if (++fsbb_ui_vout_average_index >= FSBB_UI_VOUT_DISPLAY_AVERAGE_SAMPLES)
            fsbb_ui_vout_average_index = 0U;

        if (++fsbb_ui_iout_average_index >= FSBB_UI_IOUT_DISPLAY_AVERAGE_SAMPLES)
            fsbb_ui_iout_average_index = 0U;

        fsbb_ui_vout_filtered = fsbb_ui_vout_average_sum / FSBB_UI_VOUT_DISPLAY_AVERAGE_SAMPLES;
        fsbb_ui_iout_filtered = fsbb_ui_iout_average_sum / FSBB_UI_IOUT_DISPLAY_AVERAGE_SAMPLES;
    }

    if (fsbb_ui_ready != 0)
        fsbb_ui_compose_display();

    if (fsbb_oled_ready != 0)
    {
        fsbb_oled_build_line(oled_line_index, oled_line);
        result = fsbb_oled_write_line(oled_line_index, oled_line);
        if (result != GMP_EC_OK)
        {
            fsbb_oled_ready = 0;
            gmp_base_print(TEXT_STRING("OLED display write failed; OLED disabled.\r\n"));
        }
        else
        {
            oled_line_index++;
            if (oled_line_index >= 4U)
                oled_line_index = 0U;
        }
    }

    return GMP_TASK_DONE;
}

gmp_task_status_t tsk_fsbb_ui_flush(gmp_task_t* tsk)
{
    ec_gt result;

    GMP_UNUSED_VAR(tsk);

    if (fsbb_ui_ready == 0)
        return GMP_TASK_DONE;

    result = ht16k33_update_display(&fsbb_ui_device);
    if (result != GMP_EC_OK)
    {
        fsbb_ui_ready = 0;
        gmp_base_print(TEXT_STRING("HT16K33 display write failed; UI disabled.\r\n"));
    }

    return GMP_TASK_DONE;
}
