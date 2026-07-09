#include "limits.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_pins.h"
#include "../../config/config.h"
#include <Arduino.h>

static const uint8_t limit_led_pins[4] = {
    LED_LIMIT_LEFT_PIN, LED_LIMIT_FRONT_PIN, LED_LIMIT_RIGHT_PIN, LED_LIMIT_REAR_PIN
};
static const char *limit_names[4] = {"LimL", "LimF", "LimR", "LimRe"};
static uint8_t limit_led_on[4];

static int32_t limit_x_min = -10000;
static int32_t limit_x_max = 10000;
static int32_t limit_z_min = -10000;
static int32_t limit_z_max = 10000;

void limits_ui_init(void) {
    for (uint8_t i = 0; i < 4; i++) {
        limit_led_on[i] = 0;
        LIMIT_LED_OFF(limit_led_pins[i]);
    }
}

void limits_ui_on_click(uint8_t idx) {
    if (idx > 3) return;
    limit_led_on[idx] = !limit_led_on[idx];
    if (limit_led_on[idx]) {
        LIMIT_LED_ON(limit_led_pins[idx]);
        DBG_INFO("UI", "LIM", limit_names[idx]);
    } else {
        LIMIT_LED_OFF(limit_led_pins[idx]);
        DBG_INFO("UI", "LIM", "off");
    }
}

uint8_t limits_ui_led_on(uint8_t idx) {
    return (idx < 4) ? limit_led_on[idx] : 0;
}

uint8_t limits_check(int32_t x_pos, int32_t z_pos) {
    if (x_pos < limit_x_min || x_pos > limit_x_max) return 0;
    if (z_pos < limit_z_min || z_pos > limit_z_max) return 0;
    return 1;
}

void limits_set(uint8_t axis, int32_t value, uint8_t is_max) {
    if (axis == AXIS_X) {
        if (is_max) limit_x_max = value;
        else limit_x_min = value;
    } else {
        if (is_max) limit_z_max = value;
        else limit_z_min = value;
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

int32_t limits_get_min(uint8_t axis) {
    return (axis == AXIS_X) ? limit_x_min : limit_z_min;
}

int32_t limits_get_max(uint8_t axis) {
    return (axis == AXIS_X) ? limit_x_max : limit_z_max;
}

uint8_t limits_is_active(uint8_t axis) {
    (void)axis;
    return 0;
}
