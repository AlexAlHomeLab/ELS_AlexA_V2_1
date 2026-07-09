#include "coolant_control.h"
#include "../hal/hal_pins.h"
#include <Arduino.h>

static uint8_t coolant_state = 0;

void coolant_on(void) {
    coolant_state = 1;
    digitalWrite(COOLANT_PIN, HIGH);
}

void coolant_off(void) {
    coolant_state = 0;
    digitalWrite(COOLANT_PIN, LOW);
}

void coolant_toggle(void) {
    if (coolant_state) coolant_off();
    else coolant_on();
}

uint8_t coolant_get_state(void) {
    return coolant_state;
}
