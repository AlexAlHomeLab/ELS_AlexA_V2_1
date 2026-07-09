#include "spindle_control.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_pins.h"
#include "../../config/config.h"
#include <Arduino.h>

SpindleState_t spindle_state;

static volatile uint32_t spindle_encoder_count = 0;
static uint32_t spindle_rpm = 0;
static uint32_t last_rpm_logged = 0;

void spindle_init(void) {
    spindle_state.pwm = 0;
    spindle_state.direction = 0;
    spindle_state.running = 0;
    spindle_state.rpm = 0;
    spindle_state.encoder_count = 0;
    spindle_state.last_encoder_count = 0;
    spindle_state.phase = 0;
    pinMode(SPINDLE_PWM_PIN, OUTPUT);
    pinMode(ENC_SPINDLE_A_PIN, INPUT_PULLUP);
    pinMode(ENC_SPINDLE_B_PIN, INPUT_PULLUP);
}

static void spindle_encoder_poll(void) {
    static uint8_t last_a = 0;
    uint8_t a = digitalRead(ENC_SPINDLE_A_PIN);
    uint8_t b = digitalRead(ENC_SPINDLE_B_PIN);
    if (a != last_a) {
        spindle_encoder_process(a, b);
        last_a = a;
    }
    (void)b;
}

void spindle_poll(void) {
    static unsigned long last_ms = 0;
    static uint32_t last_count = 0;

    spindle_encoder_poll();

    unsigned long now = millis();
    if (now - last_ms < 1000UL) return;

    uint32_t cnt = spindle_state.encoder_count;
    uint32_t delta = cnt - last_count;
    if (SPINDLE_ENCODER_PPR > 0) {
        spindle_rpm = (delta * 60UL) / SPINDLE_ENCODER_PPR;
    } else {
        spindle_rpm = 0;
    }
    spindle_state.rpm = spindle_rpm;
    last_count = cnt;
    last_ms = now;

    if (spindle_rpm != last_rpm_logged) {
        DBG_INFO_VAL("SPDL", "RPM", "val", spindle_rpm);
        last_rpm_logged = spindle_rpm;
    }
}

void spindle_set_speed(uint16_t pwm) {
    spindle_state.pwm = pwm;
    analogWrite(SPINDLE_PWM_PIN, pwm);
}

void spindle_set_direction(uint8_t dir) {
    spindle_state.direction = dir;
}

void spindle_start(void) {
    spindle_state.running = 1;
}

void spindle_stop(void) {
    spindle_state.running = 0;
    analogWrite(SPINDLE_PWM_PIN, 0);
    spindle_state.pwm = 0;
}

void spindle_encoder_handler(void) {
    spindle_encoder_count++;
    spindle_state.encoder_count = spindle_encoder_count;
}

void spindle_encoder_process(uint8_t a, uint8_t b) {
    (void)a;
    (void)b;
    spindle_encoder_count++;
    spindle_state.encoder_count = spindle_encoder_count;
}

uint32_t spindle_get_rpm(void) {
    return spindle_rpm;
}

uint32_t spindle_get_frequency(void) {
    return spindle_encoder_count / 10;
}
