#include "serial_config.h"
#include "../../config/config.h"
#include "../../config/config_feed.h"
#include "../../config/config_machine.h"
#include "../../config/config_storage.h"
#include "../fsm/fsm_core.h"
#include "../fsm/fsm_manager.h"
#include "../motion/motion_control.h"
#include "../process/spindle_control.h"
#include "../ui/ui_pot.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#define SERIAL_LINE_MAX 48

static char serial_line[SERIAL_LINE_MAX];
static uint8_t serial_line_len = 0;

static void reply_ok(void) {
    Serial.println(F("ok"));
}

static void reply_error(const char *msg) {
    Serial.print(F("error: "));
    Serial.println(msg);
}

static int parse_u16(const char *s, uint16_t *out) {
    if (!s || !*s) return 0;
    long v = atol(s);
    if (v < 0 || v > 65535L) return 0;
    *out = (uint16_t)v;
    return 1;
}

static int parse_float_cents(const char *s, uint16_t *out_cents) {
    if (!s || !*s) return 0;
    const char *dot = strchr(s, '.');
    if (!dot) {
        return parse_u16(s, out_cents);
    }
    long whole = atol(s);
    long frac = atol(dot + 1);
    if (whole < 0 || whole > 999L) return 0;
    if (frac < 0) frac = 0;
    if (frac < 10L && dot[1] >= '0' && dot[1] <= '9' && (dot[2] == 0)) {
        frac *= 10L;
    }
    if (frac > 99L) frac = 99L;
    *out_cents = (uint16_t)(whole * 100L + frac);
    return 1;
}

static void format_cents(char *buf, size_t len, uint16_t cents) {
    snprintf(buf, len, "%u.%02u", (unsigned)(cents / 100U), (unsigned)(cents % 100U));
}

static void format_spm(char *buf, size_t len, uint8_t axis) {
    snprintf(buf, len, "%.3f", (double)config_get_steps_per_mm(axis));
}

static int setting_get(uint8_t id, char *buf, size_t len) {
    switch (id) {
    case 0: snprintf(buf, len, "%u", config_feed_get_min_raw(FEED_RANGE_ASYNC)); return 1;
    case 1: snprintf(buf, len, "%u", config_feed_get_max_raw(FEED_RANGE_ASYNC)); return 1;
    case 2: format_cents(buf, len, config_feed_get_min_raw(FEED_RANGE_SYNC)); return 1;
    case 3: format_cents(buf, len, config_feed_get_max_raw(FEED_RANGE_SYNC)); return 1;
    case 10: snprintf(buf, len, "%u", config_get_motor_steps(AXIS_Z)); return 1;
    case 11: snprintf(buf, len, "%u", config_get_microstep(AXIS_Z)); return 1;
    case 12: format_cents(buf, len, config_get_screw_pitch(AXIS_Z)); return 1;
    case 13: snprintf(buf, len, "%u", config_get_max_speed_mm_min(AXIS_Z)); return 1;
    case 14: snprintf(buf, len, "%u", config_get_rapid_speed_mm_min(AXIS_Z)); return 1;
    case 15: snprintf(buf, len, "%u", config_get_feed_accel(AXIS_Z)); return 1;
    case 16: format_spm(buf, len, AXIS_Z); return 1;
    case 20: snprintf(buf, len, "%u", config_get_motor_steps(AXIS_X)); return 1;
    case 21: snprintf(buf, len, "%u", config_get_microstep(AXIS_X)); return 1;
    case 22: format_cents(buf, len, config_get_screw_pitch(AXIS_X)); return 1;
    case 23: snprintf(buf, len, "%u", config_get_max_speed_mm_min(AXIS_X)); return 1;
    case 24: snprintf(buf, len, "%u", config_get_rapid_speed_mm_min(AXIS_X)); return 1;
    case 25: snprintf(buf, len, "%u", config_get_feed_accel(AXIS_X)); return 1;
    case 26: format_spm(buf, len, AXIS_X); return 1;
    case 30: snprintf(buf, len, "%u", config_get_spindle_ppr()); return 1;
    case 31: snprintf(buf, len, "%u", config_get_buzzer_on()); return 1;
    default: return 0;
    }
}

static int setting_is_readonly(uint8_t id) {
    return (id == 16 || id == 26);
}

static int setting_set(uint8_t id, const char *val) {
    uint16_t u16;

    if (setting_is_readonly(id)) return 0;

    switch (id) {
    case 0:
        if (!parse_u16(val, &u16)) return 0;
        if (u16 > config_feed_get_max_raw(FEED_RANGE_ASYNC)) {
            config_feed_set_range(FEED_RANGE_ASYNC, u16, u16);
        } else {
            config_feed_set_range(FEED_RANGE_ASYNC, u16, config_feed_get_max_raw(FEED_RANGE_ASYNC));
        }
        config_feed_save();
        return 1;
    case 1:
        if (!parse_u16(val, &u16)) return 0;
        if (u16 < config_feed_get_min_raw(FEED_RANGE_ASYNC)) {
            config_feed_set_range(FEED_RANGE_ASYNC, u16, u16);
        } else {
            config_feed_set_range(FEED_RANGE_ASYNC, config_feed_get_min_raw(FEED_RANGE_ASYNC), u16);
        }
        config_feed_save();
        return 1;
    case 2:
        if (!parse_float_cents(val, &u16)) return 0;
        if (u16 > config_feed_get_max_raw(FEED_RANGE_SYNC)) {
            config_feed_set_range(FEED_RANGE_SYNC, u16, u16);
        } else {
            config_feed_set_range(FEED_RANGE_SYNC, u16, config_feed_get_max_raw(FEED_RANGE_SYNC));
        }
        config_feed_save();
        return 1;
    case 3:
        if (!parse_float_cents(val, &u16)) return 0;
        if (u16 < config_feed_get_min_raw(FEED_RANGE_SYNC)) {
            config_feed_set_range(FEED_RANGE_SYNC, u16, u16);
        } else {
            config_feed_set_range(FEED_RANGE_SYNC, config_feed_get_min_raw(FEED_RANGE_SYNC), u16);
        }
        config_feed_save();
        return 1;
    case 10:
        if (!parse_u16(val, &u16)) return 0;
        config_set_motor_steps(AXIS_Z, u16);
        config_machine_save();
        return 1;
    case 11:
        if (!parse_u16(val, &u16)) return 0;
        config_set_microstep(AXIS_Z, (uint8_t)u16);
        config_machine_save();
        return 1;
    case 12:
        if (!parse_float_cents(val, &u16)) return 0;
        config_set_screw_pitch(AXIS_Z, u16);
        config_machine_save();
        return 1;
    case 13:
        if (!parse_u16(val, &u16)) return 0;
        config_set_max_speed_mm_min(AXIS_Z, u16);
        config_machine_save();
        return 1;
    case 14:
        if (!parse_u16(val, &u16)) return 0;
        config_set_rapid_speed_mm_min(AXIS_Z, u16);
        config_machine_save();
        return 1;
    case 15:
        if (!parse_u16(val, &u16)) return 0;
        config_set_feed_accel(AXIS_Z, (uint8_t)u16);
        config_machine_save();
        return 1;
    case 20:
        if (!parse_u16(val, &u16)) return 0;
        config_set_motor_steps(AXIS_X, u16);
        config_machine_save();
        return 1;
    case 21:
        if (!parse_u16(val, &u16)) return 0;
        config_set_microstep(AXIS_X, (uint8_t)u16);
        config_machine_save();
        return 1;
    case 22:
        if (!parse_float_cents(val, &u16)) return 0;
        config_set_screw_pitch(AXIS_X, u16);
        config_machine_save();
        return 1;
    case 23:
        if (!parse_u16(val, &u16)) return 0;
        config_set_max_speed_mm_min(AXIS_X, u16);
        config_machine_save();
        return 1;
    case 24:
        if (!parse_u16(val, &u16)) return 0;
        config_set_rapid_speed_mm_min(AXIS_X, u16);
        config_machine_save();
        return 1;
    case 25:
        if (!parse_u16(val, &u16)) return 0;
        config_set_feed_accel(AXIS_X, (uint8_t)u16);
        config_machine_save();
        return 1;
    case 30:
        if (!parse_u16(val, &u16)) return 0;
        config_set_spindle_ppr(u16);
        config_machine_save();
        return 1;
    case 31:
        config_set_buzzer_on((val[0] == '1') ? 1U : 0U);
        config_save();
        return 1;
    default:
        return 0;
    }
}

static void print_setting(uint8_t id) {
    char val[20];
    if (!setting_get(id, val, sizeof(val))) return;
    Serial.print('$');
    Serial.print(id);
    Serial.print('=');
    Serial.println(val);
}

static void print_all_settings(void) {
    static const uint8_t ids[] = {
        0, 1, 2, 3,
        10, 11, 12, 13, 14, 15, 16,
        20, 21, 22, 23, 24, 25, 26,
        30, 31
    };
    for (uint8_t i = 0; i < sizeof(ids); i++) {
        print_setting(ids[i]);
    }
}

static void cmd_build_info(void) {
    Serial.print(F("[VER:"));
    Serial.print(FIRMWARE_NAME);
    Serial.print(':');
    Serial.print(FIRMWARE_STAGE);
    Serial.println(']');
    Serial.print(F("[OPT:"));
    Serial.print(PLATFORM_ARDUINO_MEGA);
    Serial.println(F(",EEPROM]"));
}

static void cmd_help(void) {
    Serial.println(F("$0-$1 async feed min/max mm/min"));
    Serial.println(F("$2-$3 sync feed min/max mm/rev"));
    Serial.println(F("$10-$15 Z motor/ustep/pitch/max/rapid/accel"));
    Serial.println(F("$16 Z steps/mm (read-only)"));
    Serial.println(F("$20-$25 X motor/ustep/pitch/max/rapid/accel"));
    Serial.println(F("$26 X steps/mm (read-only)"));
    Serial.println(F("$30 spindle PPR, $31 buzzer"));
    Serial.println(F("$$ all, $n query, $n=val set, $I info, ? status"));
}

static void cmd_status(void) {
    Serial.print(F("<MPos:"));
    Serial.print((float)motion_get_pos_steps(AXIS_X) / STEPS_PER_MM_X, 3);
    Serial.print(',');
    Serial.print((float)motion_get_pos_steps(AXIS_Z) / STEPS_PER_MM_Z, 3);
    Serial.print(F("|FSM:"));
    Serial.print(fsm_get_state());
    Serial.print(F("|M:"));
    Serial.print(fsm_manager_get_mode());
    Serial.print(F("|RPM:"));
    Serial.print(spindle_get_rpm());
    Serial.println('>');
}

static void process_dollar(char *line) {
    if (line[0] != '$') return;

    if (line[1] == 0) {
        print_all_settings();
        reply_ok();
        return;
    }

    if (line[1] == '$' && line[2] == 0) {
        print_all_settings();
        reply_ok();
        return;
    }

    if ((line[1] == 'I' || line[1] == 'i') && line[2] == 0) {
        cmd_build_info();
        reply_ok();
        return;
    }

    if ((line[1] == 'H' || line[1] == 'h') && line[2] == 0) {
        cmd_help();
        reply_ok();
        return;
    }

    char *eq = strchr(line + 1, '=');
    if (eq) {
        *eq = 0;
        long id = atol(line + 1);
        if (id < 0 || id > 255) {
            reply_error("Invalid $ statement");
            return;
        }
        if (!setting_set((uint8_t)id, eq + 1)) {
            reply_error("Invalid $ statement");
            return;
        }
        reply_ok();
        return;
    }

    long id = atol(line + 1);
    if (id < 0 || id > 255) {
        reply_error("Invalid $ statement");
        return;
    }
    char val[20];
    if (!setting_get((uint8_t)id, val, sizeof(val))) {
        reply_error("Invalid $ statement");
        return;
    }
    print_setting((uint8_t)id);
    reply_ok();
}

static void process_line(char *line) {
    if (!line || !*line) return;

    if (line[0] == '$') {
        process_dollar(line);
        return;
    }
    if (line[0] == '?' && line[1] == 0) {
        cmd_status();
        return;
    }
    reply_error("Invalid statement");
}

void serial_config_init(void) {
    serial_line_len = 0;
    Serial.print(FIRMWARE_NAME);
    Serial.print(' ');
    Serial.print(FIRMWARE_STAGE);
    Serial.println(F(" ['$' for help]"));
}

void serial_config_poll(void) {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();

        if (c == '?' && serial_line_len == 0U) {
            cmd_status();
            continue;
        }

        if (c == '\r') continue;
        if (c == '\n') {
            if (serial_line_len > 0U) {
                serial_line[serial_line_len] = 0;
                process_line(serial_line);
                serial_line_len = 0;
            }
            continue;
        }
        if (serial_line_len < (SERIAL_LINE_MAX - 1U)) {
            serial_line[serial_line_len++] = c;
        }
    }
}
