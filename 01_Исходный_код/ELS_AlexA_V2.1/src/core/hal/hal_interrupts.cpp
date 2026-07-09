#include "hal_interrupts.h"
#include "../process/spindle_control.h"
#include "../ui/ui_encoder.h"
#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>

static void mpg_interrupts_init(void) {
    EICRA |= (1 << ISC20);
    EIFR = (1 << INTF2);
    EIMSK |= (1 << INT2);
}

void encoder_interrupts_init(void) {
    EICRA |= (1 << ISC00);
    EIFR = (1 << INTF0);
    EIMSK |= (1 << INT0);
    mpg_interrupts_init();
}

void mpg_int_enable(void) {
    EIFR = (1 << INTF2);
    EIMSK |= (1 << INT2);
}

void mpg_int_disable(void) {
    EIMSK &= ~(1 << INT2);
}

ISR(INT0_vect) {
    spindle_encoder_isr_step();
}

ISR(INT2_vect) {
    hand_mpg_isr_step();
}
