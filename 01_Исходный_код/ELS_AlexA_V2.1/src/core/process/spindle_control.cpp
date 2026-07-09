#include "spindle_control.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_pins.h"
#include "../../config/config.h"
#include <Arduino.h>

SpindleState_t spindle_state;

static volatile int32_t spindle_encoder_count = 0;
static int32_t spindle_last_poll_count = 0;
static uint32_t spindle_rpm = 0;
static uint32_t last_rpm_logged = 0;

void spindle_init(void) {
    spindle_state.pwm = 0;
    spindle_state.direction = 0;
    spindle_state.running = 0;
    spindle_state.rpm = 0;
    spindle_state.encoder_count = 0;
    spindle_state.encoder_delta = 0;
    spindle_state.last_encoder_count = 0;
    spindle_state.phase = 0;
    spindle_last_poll_count = 0;
    pinMode(SPINDLE_PWM_PIN, OUTPUT);
    ENC_PORTD_INIT();
}

void spindle_encoder_isr_step(void) {
    if (!ENC_SPINDLE_B_RD()) {
        if (!ENC_SPINDLE_A_RD()) {
            spindle_state.direction = 0;
            spindle_encoder_count++;
        } else {
            spindle_state.direction = 1;
            spindle_encoder_count--;
        }
    } else {
        if (!ENC_SPINDLE_A_RD()) {
            spindle_state.direction = 1;
            spindle_encoder_count--;
        } else {
            spindle_state.direction = 0;
            spindle_encoder_count++;
        }
    }
}

void spindle_poll(void) {
    static unsigned long last_ms = 0;
    static int32_t last_count = 0;

    int32_t cnt;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        cnt = spindle_encoder_count;
    }

    spindle_state.encoder_delta = cnt - spindle_last_poll_count;
    spindle_last_poll_count = cnt;
    spindle_state.encoder_count = cnt;

    unsigned long now = millis();
    if (now - last_ms < 1000UL) return;

    int32_t delta = cnt - last_count;
    if (delta < 0) delta = -delta;
    if (SPINDLE_ENCODER_PPR > 0) {
        spindle_rpm = ((uint32_t)delta * 60UL) / SPINDLE_ENCODER_PPR;
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

void spindle_encoder_isr_tick(void) {
    spindle_encoder_isr_step();
}

void spindle_encoder_handler(void) {
    spindle_encoder_isr_step();
    spindle_state.encoder_count = spindle_encoder_count;
}

void spindle_encoder_process(uint8_t a, uint8_t b) {
    (void)a;
    (void)b;
    spindle_encoder_isr_step();
    spindle_state.encoder_count = spindle_encoder_count;
}

int32_t spindle_get_count(void) {
    int32_t cnt;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        cnt = spindle_encoder_count;
    }
    return cnt;
}

int32_t spindle_get_delta(void) {
    return spindle_state.encoder_delta;
}

uint8_t spindle_get_dir(void) {
    return spindle_state.direction;
}

uint32_t spindle_get_rpm(void) {
    return spindle_rpm;
}

uint32_t spindle_get_frequency(void) {
    int32_t cnt = spindle_get_count();
    if (cnt < 0) cnt = -cnt;
    return (uint32_t)cnt / 10U;
}
