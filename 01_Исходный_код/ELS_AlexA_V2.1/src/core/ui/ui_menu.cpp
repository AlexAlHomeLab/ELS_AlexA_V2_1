#include "ui_menu.h"
#include "ui_buttons.h"
#include "ui_lcd.h"
#include "../../config/config.h"
#include "../../config/config_feed.h"
#include "../../config/config_backlash.h"
#include "../../config/config_display.h"
#include "../../config/config_storage.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_pins.h"
#include "../motion/backlash.h"
#include "../motion/planner.h"
#include "../motion/motion_jog.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#define MENU_PARAM_COUNT 27
#define MENU_EDIT_LEN 8

#define MENU_MODE_BROWSE 0
#define MENU_MODE_EDIT 1
#define MENU_COOLDOWN_MS 400UL
#define MENU_SEL_HOLD_MS 600UL

static uint8_t menu_active = 0;
static uint8_t menu_mode = MENU_MODE_BROWSE;
static uint8_t menu_pending_save = 0;
static unsigned long menu_cooldown_ms = 0;
static uint8_t sel_down = 0;
static unsigned long sel_ms = 0;
static uint8_t sel_long_fired = 0;
static uint8_t param_idx = 0;
static uint8_t digit_pos = 0;
static char edit_buf[MENU_EDIT_LEN + 1];

static uint16_t edit_async_min = CONFIG_FEED_ASYNC_MIN_DEFAULT;
static uint16_t edit_async_max = CONFIG_FEED_ASYNC_MAX_DEFAULT;
static uint16_t edit_sync_min = CONFIG_FEED_SYNC_MIN_DEFAULT;
static uint16_t edit_sync_max = CONFIG_FEED_SYNC_MAX_DEFAULT;
static uint16_t edit_z_steps = AXIS_Z_MOTOR_STEPS_DEFAULT;
static uint8_t edit_z_ustep = AXIS_Z_MICROSTEP_DEFAULT;
static uint16_t edit_z_pitch = AXIS_Z_SCREW_PITCH_DEFAULT;
static uint16_t edit_z_max = AXIS_Z_MAX_SPEED_DEFAULT;
static uint16_t edit_z_rapid = AXIS_Z_RAPID_SPEED_DEFAULT;
static uint8_t edit_z_accel = AXIS_Z_FEED_ACCEL_DEFAULT;
static uint16_t edit_x_steps = AXIS_X_MOTOR_STEPS_DEFAULT;
static uint8_t edit_x_ustep = AXIS_X_MICROSTEP_DEFAULT;
static uint16_t edit_x_pitch = AXIS_X_SCREW_PITCH_DEFAULT;
static uint16_t edit_x_max = AXIS_X_MAX_SPEED_DEFAULT;
static uint16_t edit_x_rapid = AXIS_X_RAPID_SPEED_DEFAULT;
static uint8_t edit_x_accel = AXIS_X_FEED_ACCEL_DEFAULT;
static uint8_t edit_z_dir_inv = AXIS_Z_DIR_INVERT_DEFAULT;
static uint8_t edit_x_dir_inv = AXIS_X_DIR_INVERT_DEFAULT;
static uint16_t edit_spindle_ppr = SPINDLE_PPR_DEFAULT;
static uint16_t edit_jog_decel = JOG_DECEL_STEPS_DEFAULT;
static uint8_t edit_buzzer = CONFIG_BUZZER_DEFAULT;
static uint8_t edit_bl_auto = BACKLASH_AUTO_DEFAULT;
static uint16_t edit_bl_x = BACKLASH_X_STEPS_DEFAULT;
static uint16_t edit_bl_z = BACKLASH_Z_STEPS_DEFAULT;
static uint16_t edit_bl_speed = BACKLASH_AUTO_SPEED_DEFAULT;
static uint16_t edit_bl_min_speed = BACKLASH_MIN_SPEED_DEFAULT;
static uint8_t edit_coord_units = COORD_UNIT_DEFAULT;


typedef enum {
    PTYPE_UINT = 0,
    PTYPE_CENTS = 1,
    PTYPE_USTEP = 2,
    PTYPE_BOOL = 3,
} ParamType_t;

typedef struct {
    const char *label;
    ParamType_t type;
    uint16_t min_v;
    uint16_t max_v;
} ParamDef_t;

static const ParamDef_t param_def[MENU_PARAM_COUNT] = {
    {"aMin", PTYPE_UINT, CONFIG_FEED_ASYNC_EDIT_MIN_LOW, CONFIG_FEED_ASYNC_EDIT_MIN_HIGH},
    {"aMax", PTYPE_UINT, CONFIG_FEED_ASYNC_EDIT_MAX_LOW, CONFIG_FEED_ASYNC_EDIT_MAX_HIGH},
    {"sMin", PTYPE_CENTS, CONFIG_FEED_SYNC_EDIT_MIN_LOW, CONFIG_FEED_SYNC_EDIT_MIN_HIGH},
    {"sMax", PTYPE_CENTS, CONFIG_FEED_SYNC_EDIT_MAX_LOW, CONFIG_FEED_SYNC_EDIT_MAX_HIGH},
    {"Zstp", PTYPE_UINT, AXIS_MOTOR_STEPS_MIN, AXIS_MOTOR_STEPS_MAX},
    {"ZuSt", PTYPE_USTEP, AXIS_MICROSTEP_MIN, AXIS_MICROSTEP_MAX},
    {"Zpit", PTYPE_CENTS, AXIS_SCREW_PITCH_MIN, AXIS_SCREW_PITCH_MAX},
    {"Zmax", PTYPE_UINT, AXIS_MAX_SPEED_MIN, AXIS_MAX_SPEED_MAX},
    {"Zrap", PTYPE_UINT, AXIS_RAPID_SPEED_MIN, AXIS_RAPID_SPEED_MAX},
    {"Zacc", PTYPE_UINT, AXIS_FEED_ACCEL_MIN, AXIS_FEED_ACCEL_MAX},
    {"Xstp", PTYPE_UINT, AXIS_MOTOR_STEPS_MIN, AXIS_MOTOR_STEPS_MAX},
    {"XuSt", PTYPE_USTEP, AXIS_MICROSTEP_MIN, AXIS_MICROSTEP_MAX},
    {"Xpit", PTYPE_CENTS, AXIS_SCREW_PITCH_MIN, AXIS_SCREW_PITCH_MAX},
    {"Xmax", PTYPE_UINT, AXIS_MAX_SPEED_MIN, AXIS_MAX_SPEED_MAX},
    {"Xrap", PTYPE_UINT, AXIS_RAPID_SPEED_MIN, AXIS_RAPID_SPEED_MAX},
    {"Xacc", PTYPE_UINT, AXIS_FEED_ACCEL_MIN, AXIS_FEED_ACCEL_MAX},
    {"Spdl", PTYPE_UINT, SPINDLE_PPR_MIN, SPINDLE_PPR_MAX},
    {"Buzz", PTYPE_BOOL, 0, 1},
    {"Zinv", PTYPE_BOOL, 0, 1},
    {"Xinv", PTYPE_BOOL, 0, 1},
    {"BlAu", PTYPE_BOOL, 0, 1},
    {"BlX", PTYPE_UINT, 0, BACKLASH_STEPS_MAX},
    {"BlZ", PTYPE_UINT, 0, BACKLASH_STEPS_MAX},
    {"BlSp", PTYPE_UINT, BACKLASH_AUTO_SPEED_MIN, BACKLASH_AUTO_SPEED_MAX},
    {"BlMn", PTYPE_UINT, BACKLASH_MIN_SPEED_MIN, BACKLASH_MIN_SPEED_MAX},
    {"Jdec", PTYPE_UINT, JOG_DECEL_STEPS_MIN, JOG_DECEL_STEPS_MAX},
    {"CrdU", PTYPE_UINT, 0, 2},
};

static void ui_buzzer_beep(void) {
    if (!edit_buzzer) return;
    tone(BUZZER_PIN, 2500, 40);
}

static uint16_t *menu_param_ptr_u16(uint8_t idx) {
    switch (idx) {
    case 0: return &edit_async_min;
    case 1: return &edit_async_max;
    case 2: return &edit_sync_min;
    case 3: return &edit_sync_max;
    case 4: return &edit_z_steps;
    case 6: return &edit_z_pitch;
    case 7: return &edit_z_max;
    case 8: return &edit_z_rapid;
    case 10: return &edit_x_steps;
    case 12: return &edit_x_pitch;
    case 13: return &edit_x_max;
    case 14: return &edit_x_rapid;
    case 16: return &edit_spindle_ppr;
    case 21: return &edit_bl_x;
    case 22: return &edit_bl_z;
    case 23: return &edit_bl_speed;
    case 24: return &edit_bl_min_speed;
    case 25: return &edit_jog_decel;
    default: return NULL;
    }
}

static uint8_t *menu_param_ptr_u8(uint8_t idx) {
    switch (idx) {
    case 5: return &edit_z_ustep;
    case 9: return &edit_z_accel;
    case 11: return &edit_x_ustep;
    case 15: return &edit_x_accel;
    case 17: return &edit_buzzer;
    case 18: return &edit_z_dir_inv;
    case 19: return &edit_x_dir_inv;
    case 20: return &edit_bl_auto;
    case 26: return &edit_coord_units;
    default: return NULL;
    }
}

static uint16_t menu_param_get_u16(uint8_t idx) {
    uint16_t *p = menu_param_ptr_u16(idx);
    if (p) return *p;
    uint8_t *b = menu_param_ptr_u8(idx);
    if (b) return *b;
    return 0;
}

static void menu_param_set_u16(uint8_t idx, uint16_t v) {
    uint16_t *p = menu_param_ptr_u16(idx);
    if (p) {
        *p = v;
        return;
    }
    uint8_t *b = menu_param_ptr_u8(idx);
    if (b) *b = (uint8_t)v;
}

static void menu_load_values(void) {
    edit_async_min = config_feed_get_min_raw(FEED_RANGE_ASYNC);
    edit_async_max = config_feed_get_max_raw(FEED_RANGE_ASYNC);
    edit_sync_min = config_feed_get_min_raw(FEED_RANGE_SYNC);
    edit_sync_max = config_feed_get_max_raw(FEED_RANGE_SYNC);
    edit_z_steps = config_get_motor_steps(AXIS_Z);
    edit_z_ustep = config_get_microstep(AXIS_Z);
    edit_z_pitch = config_get_screw_pitch(AXIS_Z);
    edit_z_max = config_get_max_speed_mm_min(AXIS_Z);
    edit_z_rapid = config_get_rapid_speed_mm_min(AXIS_Z);
    edit_z_accel = config_get_feed_accel(AXIS_Z);
    edit_x_steps = config_get_motor_steps(AXIS_X);
    edit_x_ustep = config_get_microstep(AXIS_X);
    edit_x_pitch = config_get_screw_pitch(AXIS_X);
    edit_x_max = config_get_max_speed_mm_min(AXIS_X);
    edit_x_rapid = config_get_rapid_speed_mm_min(AXIS_X);
    edit_x_accel = config_get_feed_accel(AXIS_X);
    edit_z_dir_inv = config_get_dir_invert(AXIS_Z);
    edit_x_dir_inv = config_get_dir_invert(AXIS_X);
    edit_spindle_ppr = config_get_spindle_ppr();
    edit_buzzer = config_get_buzzer_on();
    edit_bl_auto = config_backlash_get_auto_on();
    edit_bl_x = config_backlash_get_steps_x();
    edit_bl_z = config_backlash_get_steps_z();
    edit_bl_speed = config_backlash_get_auto_speed();
    edit_bl_min_speed = config_backlash_get_min_speed();
    edit_jog_decel = config_get_jog_decel_steps();
    edit_coord_units = config_get_coord_units();
}

static void menu_value_to_edit_buf(uint8_t idx) {
    const ParamDef_t *d = &param_def[idx];
    uint16_t v = menu_param_get_u16(idx);

    if (d->type == PTYPE_CENTS) {
        snprintf(edit_buf, sizeof(edit_buf), "%u.%02u",
                 (unsigned)(v / 100U), (unsigned)(v % 100U));
    } else if (d->type == PTYPE_BOOL) {
        snprintf(edit_buf, sizeof(edit_buf), "%u", (unsigned)v);
    } else {
        snprintf(edit_buf, sizeof(edit_buf), "%u", (unsigned)v);
    }
}

static void menu_edit_buf_to_value(uint8_t idx) {
    const ParamDef_t *d = &param_def[idx];
    uint16_t v = 0;

    if (d->type == PTYPE_CENTS) {
        unsigned whole = 0;
        unsigned frac = 0;
        sscanf(edit_buf, "%u.%u", &whole, &frac);
        if (frac < 10U) frac *= 10U;
        if (frac > 99U) frac = 99U;
        v = (uint16_t)(whole * 100U + frac);
    } else {
        v = (uint16_t)atol(edit_buf);
    }

    if (v < d->min_v) v = d->min_v;
    if (v > d->max_v) v = d->max_v;
    menu_param_set_u16(idx, v);
    menu_value_to_edit_buf(idx);
}

static void menu_edit_find_first_digit(void) {
    for (uint8_t i = 0; edit_buf[i]; i++) {
        if (edit_buf[i] >= '0' && edit_buf[i] <= '9') {
            digit_pos = i;
            return;
        }
    }
    digit_pos = 0;
}

static void menu_edit_next_digit(int8_t dir) {
    int16_t i = (int16_t)digit_pos;
    if (dir > 0) {
        for (i++; edit_buf[i]; i++) {
            if (edit_buf[i] >= '0' && edit_buf[i] <= '9') {
                digit_pos = (uint8_t)i;
                return;
            }
        }
    } else {
        for (i--; i >= 0; i--) {
            if (edit_buf[i] >= '0' && edit_buf[i] <= '9') {
                digit_pos = (uint8_t)i;
                return;
            }
        }
    }
}

static uint8_t menu_edit_is_digit(uint8_t pos) {
    return (edit_buf[pos] >= '0' && edit_buf[pos] <= '9');
}

static uint8_t menu_edit_int_digit_count(void) {
    uint8_t n = 0;
    for (uint8_t i = 0; edit_buf[i] && edit_buf[i] != '.'; i++) {
        if (menu_edit_is_digit(i)) n++;
    }
    return n;
}

static uint8_t menu_edit_total_digit_count(void) {
    uint8_t n = 0;
    for (uint8_t i = 0; edit_buf[i]; i++) {
        if (menu_edit_is_digit(i)) n++;
    }
    return n;
}

static void menu_edit_fixup_digit_pos(void) {
    if (menu_edit_is_digit(digit_pos)) return;
    menu_edit_next_digit(-1);
    if (!menu_edit_is_digit(digit_pos)) {
        menu_edit_find_first_digit();
    }
}

static void menu_edit_shift_left(uint8_t from) {
    uint8_t i = from;
    while (edit_buf[i]) {
        edit_buf[i] = edit_buf[i + 1U];
        i++;
    }
}

static uint8_t menu_edit_in_frac(void) {
    const char *dot = strchr(edit_buf, '.');
    if (!dot) return 0;
    return (digit_pos > (uint8_t)(dot - edit_buf));
}

static void menu_edit_cents_normalize(void) {
    char f0 = '0';
    char f1 = '0';
    const char *dot = strchr(edit_buf, '.');
    if (dot) {
        if (dot[1] >= '0' && dot[1] <= '9') f0 = dot[1];
        if (dot[2] >= '0' && dot[2] <= '9') f1 = dot[2];
    }
    char ibuf[MENU_EDIT_LEN];
    uint8_t n = 0;
    const char *end = dot ? dot : edit_buf + strlen(edit_buf);
    for (const char *p = edit_buf; p < end && n < (sizeof(ibuf) - 1U); p++) {
        if (*p >= '0' && *p <= '9') ibuf[n++] = *p;
    }
    if (n == 0) ibuf[n++] = '0';
    ibuf[n] = 0;
    snprintf(edit_buf, sizeof(edit_buf), "%s.%c%c", ibuf, f0, f1);
}

static uint8_t menu_edit_can_delete_digit(void) {
    const ParamDef_t *d = &param_def[param_idx];

    if (!menu_edit_is_digit(digit_pos)) return 0;
    if (d->type == PTYPE_CENTS) {
        if (menu_edit_in_frac()) return 0;
        return (menu_edit_int_digit_count() > 1U);
    }
    return (menu_edit_total_digit_count() > 1U);
}

static void menu_edit_delete_digit(void) {
    if (!menu_edit_can_delete_digit()) return;
    menu_edit_shift_left(digit_pos);
    menu_edit_fixup_digit_pos();
}

static void menu_edit_insert_digit_zero(void) {
    uint8_t len = (uint8_t)strlen(edit_buf);
    if (len >= MENU_EDIT_LEN) return;
    for (uint8_t i = len + 1U; i > digit_pos + 1U; i--) {
        edit_buf[i] = edit_buf[i - 1U];
    }
    edit_buf[digit_pos + 1U] = '0';
    edit_buf[len + 1U] = 0;
}

static void menu_edit_uint_digit_delta(int8_t dir) {
    char c = edit_buf[digit_pos];

    if (dir > 0) {
        if (c == '9') {
            edit_buf[digit_pos] = '0';
            menu_edit_insert_digit_zero();
        } else {
            edit_buf[digit_pos] = (char)(c + 1);
        }
    } else {
        if (c == '0') {
            menu_edit_delete_digit();
        } else {
            edit_buf[digit_pos] = (char)(c - 1);
        }
    }
}

static void menu_edit_cents_digit_delta(int8_t dir) {
    char c = edit_buf[digit_pos];

    if (menu_edit_in_frac()) {
        if (dir > 0) {
            edit_buf[digit_pos] = (c == '9') ? '0' : (char)(c + 1);
        } else {
            edit_buf[digit_pos] = (c == '0') ? '9' : (char)(c - 1);
        }
        return;
    }

    if (dir > 0) {
        if (c == '9') {
            edit_buf[digit_pos] = '0';
            menu_edit_insert_digit_zero();
        } else {
            edit_buf[digit_pos] = (char)(c + 1);
        }
    } else {
        if (c == '0') {
            if (!menu_edit_can_delete_digit()) return;
            menu_edit_shift_left(digit_pos);
            menu_edit_fixup_digit_pos();
        } else {
            edit_buf[digit_pos] = (char)(c - 1);
        }
    }
    menu_edit_cents_normalize();
    menu_edit_fixup_digit_pos();
}

static void menu_edit_digit_delta(int8_t dir) {
    const ParamDef_t *d = &param_def[param_idx];

    if (d->type == PTYPE_USTEP) {
        static const uint8_t tbl[] = {1, 2, 4, 8, 16, 32};
        uint8_t ms = (uint8_t)menu_param_get_u16(param_idx);
        uint8_t i;
        for (i = 0; i < 6U; i++) {
            if (tbl[i] == ms) break;
        }
        if (dir > 0 && i < 5U) i++;
        if (dir < 0 && i > 0U) i--;
        menu_param_set_u16(param_idx, tbl[i]);
        menu_value_to_edit_buf(param_idx);
        menu_edit_find_first_digit();
        return;
    }

    if (d->type == PTYPE_BOOL) {
        menu_param_set_u16(param_idx, menu_param_get_u16(param_idx) ? 0U : 1U);
        menu_value_to_edit_buf(param_idx);
        menu_edit_find_first_digit();
        return;
    }

    if (!menu_edit_is_digit(digit_pos)) return;

    if (d->type == PTYPE_CENTS) {
        menu_edit_cents_digit_delta(dir);
        return;
    }
    menu_edit_uint_digit_delta(dir);
}

static void menu_format_list_line(uint8_t idx, char *buf, size_t len) {
    char val[12];
    const ParamDef_t *d = &param_def[idx];

    if (menu_mode == MENU_MODE_EDIT && idx == param_idx) {
        strncpy(val, edit_buf, sizeof(val) - 1U);
        val[sizeof(val) - 1U] = 0;
    } else {
        uint16_t v = menu_param_get_u16(idx);

        if (d->type == PTYPE_CENTS) {
            snprintf(val, sizeof(val), "%u.%02u", (unsigned)(v / 100U), (unsigned)(v % 100U));
        } else if (d->type == PTYPE_BOOL) {
            snprintf(val, sizeof(val), "%s", v ? "ON" : "OFF");
        } else {
            snprintf(val, sizeof(val), "%u", (unsigned)v);
        }
    }

    char mark = (idx == param_idx) ? '>' : ' ';
    snprintf(buf, len, "%c%-5s %s", mark, d->label, val);
}

#define MENU_VAL_COL 7U

void ui_menu_init(void) {
    menu_active = 0;
    menu_mode = MENU_MODE_BROWSE;
    menu_pending_save = 0;
    menu_cooldown_ms = 0;
    sel_down = 0;
    sel_ms = 0;
    sel_long_fired = 0;
    param_idx = 0;
    menu_load_values();
}

uint8_t ui_menu_is_active(void) {
    return menu_active;
}

void ui_menu_lcd(void) {
    char line[LCD_COLS + 1];
    uint8_t scroll = 0;

    if (param_idx > 1U) scroll = (uint8_t)(param_idx - 1U);
    if (scroll > (MENU_PARAM_COUNT - 3U)) scroll = (uint8_t)(MENU_PARAM_COUNT - 3U);

    menu_format_list_line((uint8_t)(scroll + 0U), line, sizeof(line));
    ui_lcd_set_line(0, line);
    menu_format_list_line((uint8_t)(scroll + 1U), line, sizeof(line));
    ui_lcd_set_line(1, line);
    menu_format_list_line((uint8_t)(scroll + 2U), line, sizeof(line));
    ui_lcd_set_line(2, line);

    if (menu_mode == MENU_MODE_EDIT) {
        uint8_t cursor_row = 0xFF;
        if (param_idx == scroll) cursor_row = 0;
        else if (param_idx == (uint8_t)(scroll + 1U)) cursor_row = 1;
        else if (param_idx == (uint8_t)(scroll + 2U)) cursor_row = 2;

        if (cursor_row < LCD_ROWS) {
            uint8_t col = (uint8_t)(MENU_VAL_COL + digit_pos);
            if (col < LCD_COLS) {
                ui_lcd_set_cursor(cursor_row, col);
            } else {
                ui_lcd_clear_cursor();
            }
        } else {
            ui_lcd_clear_cursor();
        }
        ui_lcd_set_line(3, "Hold=ok L/R U/D    ");
    } else {
        ui_lcd_set_line(3, "Sel=edit Hold=exit");
        ui_lcd_clear_cursor();
    }
}

static uint8_t menu_cooldown_ok(void) {
    return (millis() - menu_cooldown_ms) >= MENU_COOLDOWN_MS;
}

static void menu_open(void) {
    menu_active = 1;
    menu_mode = MENU_MODE_BROWSE;
    param_idx = 0;
    menu_load_values();
    menu_cooldown_ms = millis();
    DBG_INFO("UI", "MENU", "open");
}

static void menu_save_all(void) {
    if (edit_async_min > edit_async_max) {
        uint16_t t = edit_async_min;
        edit_async_min = edit_async_max;
        edit_async_max = t;
    }
    if (edit_sync_min > edit_sync_max) {
        uint16_t t = edit_sync_min;
        edit_sync_min = edit_sync_max;
        edit_sync_max = t;
    }

    config_feed_set_range(FEED_RANGE_ASYNC, edit_async_min, edit_async_max);
    config_feed_set_range(FEED_RANGE_SYNC, edit_sync_min, edit_sync_max);
    config_feed_save();

    config_set_motor_steps(AXIS_Z, edit_z_steps);
    config_set_microstep(AXIS_Z, edit_z_ustep);
    config_set_screw_pitch(AXIS_Z, edit_z_pitch);
    config_set_max_speed_mm_min(AXIS_Z, edit_z_max);
    config_set_rapid_speed_mm_min(AXIS_Z, edit_z_rapid);
    config_set_feed_accel(AXIS_Z, edit_z_accel);
    config_set_motor_steps(AXIS_X, edit_x_steps);
    config_set_microstep(AXIS_X, edit_x_ustep);
    config_set_screw_pitch(AXIS_X, edit_x_pitch);
    config_set_max_speed_mm_min(AXIS_X, edit_x_max);
    config_set_rapid_speed_mm_min(AXIS_X, edit_x_rapid);
    config_set_feed_accel(AXIS_X, edit_x_accel);
    config_set_dir_invert(AXIS_Z, edit_z_dir_inv);
    config_set_dir_invert(AXIS_X, edit_x_dir_inv);
    config_set_jog_decel_steps(edit_jog_decel);
    config_machine_save();

    config_backlash_set_auto_on(edit_bl_auto);
    config_backlash_set_steps_x(edit_bl_x);
    config_backlash_set_steps_z(edit_bl_z);
    config_backlash_set_auto_speed(edit_bl_speed);
    config_backlash_set_min_speed(edit_bl_min_speed);
    config_backlash_save();
    backlash_reload_steps();

    config_set_coord_units((uint8_t)edit_coord_units);
    config_display_save();

    config_set_buzzer_on(edit_buzzer);
    config_save();
}

static void menu_exit(void) {
    menu_active = 0;
    menu_mode = MENU_MODE_BROWSE;
    ui_lcd_clear_cursor();
    planner_stop_all();
    ui_buttons_reset_joy();
    motion_jog_resume();
    menu_cooldown_ms = millis();
    DBG_INFO("UI", "MENU", "exit");
}

static void menu_enter_edit(void) {
    menu_mode = MENU_MODE_EDIT;
    menu_value_to_edit_buf(param_idx);
    menu_edit_find_first_digit();
}

static void menu_confirm_edit(void) {
    menu_edit_buf_to_value(param_idx);
    menu_mode = MENU_MODE_BROWSE;
    ui_lcd_clear_cursor();
    menu_cooldown_ms = millis();
    ui_buzzer_beep();
}

static void menu_poll_select(void) {
    ButtonClicks_t cl = ui_buttons_get_clicks();
    ButtonState_t st = ui_buttons_get_state();
    unsigned long now = millis();

    if (st.select) {
        if (!sel_down) {
            sel_down = 1;
            sel_ms = now;
            sel_long_fired = 0;
        } else if (!sel_long_fired && (now - sel_ms) >= MENU_SEL_HOLD_MS) {
            sel_long_fired = 1;
            if (!menu_active) {
                if (menu_cooldown_ok()) {
                    menu_open();
                }
            } else if (menu_mode == MENU_MODE_EDIT) {
                menu_confirm_edit();
            } else {
                menu_pending_save = 1;
                ui_buzzer_beep();
                menu_exit();
            }
        }
        return;
    }

    if (sel_down) {
        /* Короткое отпускание: edit. Не полагаемся на cl.select — EncButton
         * при удержании >= EB_HOLD_TIME (600 ms) не даёт click(). */
        if (!sel_long_fired && menu_active && menu_mode == MENU_MODE_BROWSE) {
            menu_enter_edit();
        }
        sel_down = 0;
        sel_long_fired = 0;
    } else if (cl.select && menu_active && menu_mode == MENU_MODE_BROWSE) {
        /* Короткий клик, если нажатие не попало в poll (sel_down не успел). */
        menu_enter_edit();
    }
}

void ui_menu_poll(void) {
    if (!menu_active && menu_pending_save) {
        menu_pending_save = 0;
        menu_save_all();
        ui_buttons_reset_joy();
    }

    menu_poll_select();

    if (!menu_active) {
        return;
    }

    ButtonClicks_t cl = ui_buttons_get_clicks();

    if (menu_mode == MENU_MODE_BROWSE) {
        if (cl.up && param_idx > 0) param_idx--;
        if (cl.down && param_idx < (MENU_PARAM_COUNT - 1U)) param_idx++;
        return;
    }

    if (cl.left) menu_edit_next_digit(-1);
    if (cl.right) menu_edit_next_digit(1);
    if (cl.up) menu_edit_digit_delta(1);
    if (cl.down) menu_edit_digit_delta(-1);
}
