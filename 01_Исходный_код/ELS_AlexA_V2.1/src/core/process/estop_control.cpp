#include "estop_control.h"
#include "../debug/debug_serial.h"
#include "../fsm/fsm_core.h"
#include "../hal/hal_buzzer.h"
#include "../hal/hal_pins.h"
#include "../motion/motion_control.h"
#include "../motion/planner.h"
#include <Arduino.h>

static uint8_t estop_state = 0;
static uint8_t estop_triggered = 0;

void estop_check(void) {
    if (PIN_ACTIVE_LOW(ESTOP_BTN_PIN)) {
        estop_trigger();
    }
}

void estop_trigger(void) {
    if (estop_triggered) return;
    estop_triggered = 1;
    estop_state = 1;
    fsm_force_error();
    analogWrite(SPINDLE_PWM_PIN, 0);
    /* motor_en_x_release(); — EN_X при E-Stop пока без реализации */
    DBG_INFO("SYS", "ESTOP", "trigger");
    hal_buzzer_estop_signal();
}

void estop_reset(void) {
    if (!estop_triggered) return;
    estop_triggered = 0;
    estop_state = 0;
    fsm_recover();
    DBG_INFO("SYS", "ESTOP", "reset");
}

uint8_t estop_get_state(void) {
    return estop_state;
}

uint8_t estop_is_triggered(void) {
    return estop_triggered;
}
