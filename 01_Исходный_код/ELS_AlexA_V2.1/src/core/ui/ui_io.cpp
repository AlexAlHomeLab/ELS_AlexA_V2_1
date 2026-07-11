#include "ui_io.h"
#include "../hal/hal_pins.h"
#include "../hal/hal_ports.h"
#include <Arduino.h>
#include <avr/io.h>

bool EB_read(uint8_t pin) {
    switch (pin) {
        case BTN_DOWN_PIN:     return (PINF >> 0) & 1;
        case BTN_UP_PIN:       return (PINF >> 1) & 1;
        case BTN_RIGHT_PIN:    return (PINF >> 2) & 1;
        case BTN_LEFT_PIN:     return (PINF >> 3) & 1;
        case BTN_SELECT_PIN:   return (PINF >> 4) & 1;
        case JOY_LEFT_PIN:     return (PINK >> 0) & 1;
        case JOY_RIGHT_PIN:    return (PINK >> 1) & 1;
        case JOY_UP_PIN:       return (PINK >> 2) & 1;
        case JOY_DOWN_PIN:     return (PINK >> 3) & 1;
        case JOY_RAPID_PIN:    return (PINK >> 4) & 1;
        case SUB_INT_PIN:      return (PINK >> 5) & 1;
        case SUB_MAN_PIN:      return (PINK >> 6) & 1;
        case SUB_EXT_PIN:      return (PINK >> 7) & 1;
        case LIMIT_LEFT_PIN:   return (PINA >> 6) & 1;  /* D28 PA6 — левый на плате 7e2 */
        case LIMIT_FRONT_PIN:  return (PINA >> 2) & 1;
        case LIMIT_RIGHT_PIN:  return (PINA >> 4) & 1;
        case LIMIT_REAR_PIN:   return (PINA >> 0) & 1;  /* D22 PA0 — задний на плате 7e2 */
        case HAND_AXIS_Z_PIN:  return (PINE >> 4) & 1;
        case HAND_AXIS_X_PIN:  return (PINE >> 5) & 1;
        case HAND_SCALE_X1_PIN:  return (PINJ >> 1) & 1;
        case HAND_SCALE_X10_PIN: return (PINJ >> 0) & 1;
        default: return hal_pin_read(pin) != 0;
    }
}

void EB_mode(uint8_t pin, uint8_t mode) {
    pinMode(pin, mode);
}

uint32_t EB_uptime(void) {
    return millis();
}
