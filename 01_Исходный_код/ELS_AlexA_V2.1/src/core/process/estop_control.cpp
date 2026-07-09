#include "estop_control.h"
#include "../fsm/fsm_core.h"
#include "../hal/hal_pins.h"
#include "../hal/hal_ports.h"
#include "../motion/motion_control.h"
#include "../motion/planner.h"
#include <Arduino.h>

static uint8_t estop_state = 0;
static uint8_t estop_triggered = 0;

#include <avr/io.h>

void estop_check(void) {
    if (((PINF >> 4) & 1) == 0) {
        estop_trigger();
    }
}

void estop_trigger(void) {
    if (estop_triggered) return;
    estop_triggered = 1;
    estop_state = 1;
    fsm_force_error();
    analogWrite(SPINDLE_PWM_PIN, 0);
    for (int i = 0; i < 5; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(100);
        digitalWrite(BUZZER_PIN, LOW);
        delay(100);
    }
}

void estop_reset(void) {
    estop_triggered = 0;
    estop_state = 0;
    fsm_recover();
}

uint8_t estop_get_state(void) {
    return estop_state;
}

uint8_t estop_is_triggered(void) {
    return estop_triggered;
}
