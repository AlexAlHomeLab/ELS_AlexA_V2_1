#include "ui_switches.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_pins.h"
#include "../../config/config_defs.h"
#include <Arduino.h>

static SwitchState_t sw_state;
static uint8_t last_mode = 0;
static uint8_t last_submode = 0xFF;

static const char *mode_names[] = {
    "Async", "Sync", "Thread", "Chamfer", "Cone", "Sphere", "Divider", "Grind"
};

static uint8_t read_mode(void) {
    for (uint8_t i = 0; i < 8; i++) {
        if (PIN_ACTIVE_LOW(MODE_PIN_1 + i)) {
            return i + 1;
        }
    }
    return 1;
}

static uint8_t read_submode(void) {
    if (PIN_ACTIVE_LOW(SUB_MAN_PIN)) return SUB_MANUAL;
    if (PIN_ACTIVE_LOW(SUB_EXT_PIN)) return SUB_EXT;
    if (PIN_ACTIVE_LOW(SUB_INT_PIN)) return SUB_INT;
    return SUB_MANUAL;
}

static uint8_t read_mpg_axis(void) {
    if (PIN_ACTIVE_LOW(HAND_AXIS_X_PIN)) return AXIS_X;
    if (PIN_ACTIVE_LOW(HAND_AXIS_Z_PIN)) return AXIS_Z;
    return AXIS_Z;
}

static uint8_t read_mpg_scale(void) {
    if (PIN_ACTIVE_LOW(HAND_SCALE_X10_PIN)) return 1;
    if (PIN_ACTIVE_LOW(HAND_SCALE_X1_PIN)) return 0;
    return 0;
}

void ui_switches_init(void) {
    sw_state.mode = read_mode();
    sw_state.submode = read_submode();
    sw_state.mpg_axis = read_mpg_axis();
    sw_state.mpg_scale = read_mpg_scale();
    last_mode = sw_state.mode;
    last_submode = sw_state.submode;
}

void ui_switches_poll(void) {
    uint8_t mode = read_mode();
    uint8_t sub = read_submode();

    if (mode != last_mode) {
        DBG_INFO_VAL("UI", "MODE", ui_switches_get_mode_name(mode), mode);
        last_mode = mode;
    }
    if (sub != last_submode) {
        const char *sub_name = "Man";
        if (sub == SUB_EXT) sub_name = "Ext";
        else if (sub == SUB_INT) sub_name = "Int";
        DBG_INFO("UI", "SUB", sub_name);
        last_submode = sub;
    }

    sw_state.mode = mode;
    sw_state.submode = sub;
    sw_state.mpg_axis = read_mpg_axis();
    sw_state.mpg_scale = read_mpg_scale();
}

SwitchState_t ui_switches_get_state(void) {
    return sw_state;
}

const char *ui_switches_get_mode_name(uint8_t mode) {
    if (mode < 1 || mode > 8) return "?";
    return mode_names[mode - 1];
}
