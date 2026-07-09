#include "ui_encoder.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_interrupts.h"
#include "../hal/hal_pins.h"
#include <Arduino.h>

static volatile int32_t mpg_position = 0;
static volatile int32_t mpg_increment = 0;

void ui_encoder_init(void) {
    mpg_position = 0;
    mpg_increment = 0;
    pinMode(ENC_HAND_A_PIN, INPUT_PULLUP);
    pinMode(ENC_HAND_B_PIN, INPUT_PULLUP);
    encoder_interrupts_init();
    mpg_interrupts_init();
}

void mpg_process(uint8_t a, uint8_t b) {
    static uint8_t last_a = 0;
    if (a != last_a) {
        if (a == b) {
            mpg_position++;
            mpg_increment = 1;
        } else {
            mpg_position--;
            mpg_increment = -1;
        }
        last_a = a;
    }
}

void ui_encoder_poll(void) {
    static uint8_t last_a = 0;
    static uint8_t last_b = 0;
    uint8_t a = digitalRead(ENC_HAND_A_PIN);
    uint8_t b = digitalRead(ENC_HAND_B_PIN);

    if (a != last_a || b != last_b) {
        mpg_process(a, b);
        int32_t delta = ui_encoder_get_mpg_delta();
        if (delta != 0) {
            DBG_INFO_VAL("UI", "MPG", "delta", (uint32_t)(delta > 0 ? delta : -delta));
        }
        last_a = a;
        last_b = b;
    }
}

int32_t ui_encoder_get_mpg_pos(void) {
    return mpg_position;
}

int32_t ui_encoder_get_mpg_delta(void) {
    int32_t val = mpg_increment;
    mpg_increment = 0;
    return val;
}

void ui_encoder_reset_mpg(void) {
    mpg_position = 0;
}
