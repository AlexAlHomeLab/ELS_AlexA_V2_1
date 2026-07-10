#include "limits.h"
#include "motion_control.h"
#include "motion_jog.h"
#include "stepper_gen.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_pins.h"
#include "../../config/config.h"
#include "../../config/config_storage.h"
#include <Arduino.h>

#define LIMIT_OFF_MIN (-2000000L)
#define LIMIT_OFF_MAX  2000000L

static const uint8_t limit_led_pins[4] = {
    LED_LIMIT_LEFT_PIN, LED_LIMIT_FRONT_PIN, LED_LIMIT_RIGHT_PIN, LED_LIMIT_REAR_PIN
};
static const char *limit_names[4] = {"LimL", "LimF", "LimR", "LimRe"};

/* LimL/LimR — Z, LimF/LimRe — X (как 7e2) */
static uint8_t limit_active[4];
static int32_t limit_pos[4];

static void limit_beep(void) {
    if (!config_get_buzzer_on()) return;
    tone(BUZZER_PIN, 2500, 40);
}

static void limit_beep_double(void) {
    if (!config_get_buzzer_on()) return;
    tone(BUZZER_PIN, 2500, 40);
    delay(45);
    noTone(BUZZER_PIN);
    delay(80);
    tone(BUZZER_PIN, 2500, 40);
}

static uint8_t limit_idx_to_axis(uint8_t idx) {
    return (idx == 0 || idx == 2) ? AXIS_Z : AXIS_X;
}

static int32_t read_axis_pos(uint8_t axis) {
    return motion_get_pos_steps(axis);
}

void limits_ui_init(void) {
    for (uint8_t i = 0; i < 4; i++) {
        limit_active[i] = 0;
        limit_pos[i] = 0;
        LIMIT_LED_OFF(limit_led_pins[i]);
    }
}

static uint8_t can_set_limit(uint8_t idx, int32_t pos) {
    if (idx == 0) {
        if (limit_active[2] && pos >= limit_pos[2]) return 0;
    } else if (idx == 2) {
        if (limit_active[0] && pos <= limit_pos[0]) return 0;
    } else if (idx == 1) {
        if (limit_active[3] && pos <= limit_pos[3]) return 0;
    } else if (idx == 3) {
        if (limit_active[1] && pos >= limit_pos[1]) return 0;
    }
    return 1;
}

void limits_ui_on_click(uint8_t idx) {
    if (idx > 3) return;

    if (limit_active[idx]) {
        limit_active[idx] = 0;
        LIMIT_LED_OFF(limit_led_pins[idx]);
        DBG_INFO("UI", "LIM", "off");
        limit_beep();
        return;
    }

    int32_t pos = 0;
    if (idx == 0 || idx == 2) {
        pos = read_axis_pos(AXIS_Z);
    } else {
        pos = read_axis_pos(AXIS_X);
    }

    if (!can_set_limit(idx, pos)) return;

    limit_active[idx] = 1;
    limit_pos[idx] = pos;
    LIMIT_LED_ON(limit_led_pins[idx]);
    DBG_INFO_VAL_I32("UI", "LIM", limit_names[idx], pos);
    limit_beep();
}

void limits_ui_on_hold(uint8_t idx) {
    if (idx > 3) return;
    motion_jog_zero_axis(limit_idx_to_axis(idx));
    DBG_INFO("UI", "ZERO", limit_names[idx]);
    limit_beep_double();
}

uint8_t limits_ui_go_target(uint8_t idx, uint8_t *axis, int32_t *target) {
    if (idx > 3 || !limit_active[idx] || axis == NULL || target == NULL) return 0;
    *axis = limit_idx_to_axis(idx);
    *target = limit_pos[idx];
    return 1;
}

uint8_t limits_ui_go_target_dir(uint8_t axis, int8_t sign, uint8_t *idx, int32_t *target) {
    if (axis > AXIS_Z || sign == 0 || idx == NULL || target == NULL) return 0;

    uint8_t lim_idx = (sign > 0)
        ? ((axis == AXIS_Z) ? 2 : 1)
        : ((axis == AXIS_Z) ? 0 : 3);

    if (!limit_active[lim_idx]) return 0;

    int32_t cur = read_axis_pos(axis);
    int32_t tgt = limit_pos[lim_idx];
    if (sign > 0 && cur >= tgt) return 0;
    if (sign < 0 && cur <= tgt) return 0;

    *idx = lim_idx;
    *target = tgt;
    return 1;
}

uint8_t limits_ui_led_on(uint8_t idx) {
    return (idx < 4) ? limit_active[idx] : 0;
}

char limits_lcd_marker(uint8_t axis) {
    int32_t pos = read_axis_pos(axis);

    if (axis == AXIS_X) {
        if (limit_active[3] && pos == limit_pos[3]) return 'B';
        if (limit_active[1] && pos == limit_pos[1]) return 'F';
    } else if (axis == AXIS_Z) {
        if (limit_active[0] && pos == limit_pos[0]) return '<';
        if (limit_active[2] && pos == limit_pos[2]) return '>';
    }
    return ' ';
}

void limits_rebase_axis(uint8_t axis, int32_t old_pos) {
    if (axis == AXIS_Z) {
        if (limit_active[0]) limit_pos[0] -= old_pos;
        if (limit_active[2]) limit_pos[2] -= old_pos;
    } else if (axis == AXIS_X) {
        if (limit_active[1]) limit_pos[1] -= old_pos;
        if (limit_active[3]) limit_pos[3] -= old_pos;
    }
}

int32_t limits_get_min(uint8_t axis) {
    if (axis == AXIS_Z) {
        return limit_active[0] ? limit_pos[0] : LIMIT_OFF_MIN;
    }
    return limit_active[3] ? limit_pos[3] : LIMIT_OFF_MIN;
}

int32_t limits_get_max(uint8_t axis) {
    if (axis == AXIS_Z) {
        return limit_active[2] ? limit_pos[2] : LIMIT_OFF_MAX;
    }
    return limit_active[1] ? limit_pos[1] : LIMIT_OFF_MAX;
}

uint8_t limits_is_active(uint8_t axis) {
    if (axis == AXIS_Z) {
        return limit_active[0] || limit_active[2];
    }
    return limit_active[1] || limit_active[3];
}

uint8_t limits_check(int32_t x_pos, int32_t z_pos) {
    if (x_pos < limits_get_min(AXIS_X) || x_pos > limits_get_max(AXIS_X)) return 0;
    if (z_pos < limits_get_min(AXIS_Z) || z_pos > limits_get_max(AXIS_Z)) return 0;
    return 1;
}

int32_t limits_clamp_steps(uint8_t axis, int32_t target) {
    int32_t mn = limits_get_min(axis);
    int32_t mx = limits_get_max(axis);
    if (target < mn) return mn;
    if (target > mx) return mx;
    return target;
}

void limits_set(uint8_t axis, int32_t value, uint8_t is_max) {
    if (axis == AXIS_Z) {
        if (is_max) {
            limit_active[2] = 1;
            limit_pos[2] = value;
            LIMIT_LED_ON(limit_led_pins[2]);
        } else {
            limit_active[0] = 1;
            limit_pos[0] = value;
            LIMIT_LED_ON(limit_led_pins[0]);
        }
    } else {
        if (is_max) {
            limit_active[1] = 1;
            limit_pos[1] = value;
            LIMIT_LED_ON(limit_led_pins[1]);
        } else {
            limit_active[3] = 1;
            limit_pos[3] = value;
            LIMIT_LED_ON(limit_led_pins[3]);
        }
    }
}

uint8_t limits_hardware_check(void) {
    if (PIN_ACTIVE_LOW(LIMIT_LEFT_PIN)) return 0;
    if (PIN_ACTIVE_LOW(LIMIT_RIGHT_PIN)) return 0;
    if (PIN_ACTIVE_LOW(LIMIT_UP_PIN)) return 0;
    if (PIN_ACTIVE_LOW(LIMIT_DOWN_PIN)) return 0;
    return 1;
}

void limits_set_axis(uint8_t axis, uint8_t is_min) {
    (void)axis;
    (void)is_min;
}
