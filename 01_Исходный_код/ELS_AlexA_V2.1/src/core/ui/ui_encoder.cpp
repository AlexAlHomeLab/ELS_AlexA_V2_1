#include "ui_encoder.h"
#include "../hal/hal_pins.h"
#include <Arduino.h>
#include <avr/interrupt.h>

/* РГИ: тики INT2 (7e2), delta в loop. EncButton не используется. */

static volatile int32_t hand_count = 0;
static int32_t hand_count_last = 0;

void ui_encoder_init(void) {
    ENC_PORTD_INIT();
    hand_count = 0;
    hand_count_last = 0;
}

extern "C" void hand_mpg_isr_step(void) {
    if (!ENC_HAND_B_RD()) {
        if (!ENC_HAND_A_RD()) {
            hand_count--;
        }
    } else {
        if (!ENC_HAND_A_RD()) {
            hand_count++;
        }
    }
}

void ui_encoder_poll(void) {
}

int32_t ui_encoder_get_mpg_pos(void) {
    int32_t snap;
    cli();
    snap = hand_count;
    sei();
    return snap;
}

int32_t ui_encoder_peek_mpg_delta(void) {
    int32_t snap;
    cli();
    snap = hand_count;
    sei();
    return snap - hand_count_last;
}

int32_t ui_encoder_get_mpg_delta(void) {
    int32_t snap;
    cli();
    snap = hand_count;
    sei();
    int32_t delta = snap - hand_count_last;
    hand_count_last = snap;
    return delta;
}

int32_t ui_encoder_consume_mpg_tick(void) {
    int32_t snap;
    int32_t d;

    cli();
    snap = hand_count;
    d = snap - hand_count_last;
    if (d > 0) {
        hand_count_last++;
        sei();
        return 1;
    }
    if (d < 0) {
        hand_count_last--;
        sei();
        return -1;
    }
    sei();
    return 0;
}

void ui_encoder_reset_mpg(void) {
    cli();
    hand_count = 0;
    hand_count_last = 0;
    sei();
}
