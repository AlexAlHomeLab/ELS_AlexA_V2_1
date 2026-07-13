#include "hal_timers.h"
#include "../motion/stepper_gen.h"
#include "../process/spindle_control.h"
#include "../ui/ui_pot.h"
#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

static volatile uint32_t stepper_ticks = 0;

void timer1_init(uint16_t period_us) {
    cli();
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11);
    OCR1A = period_us * 2;
    TIMSK1 = (1 << OCIE1A);
    sei();
}

ISR(TIMER1_COMPA_vect) {
    /* Без sei() внутри: иначе реентrant TIMER1/LCD и переполнение стека */
    stepper_ticks++;
    stepper_generate_steps();
}

void timer2_init(void) {
    cli();
    TCCR2A = 0;
    TCCR2B = (1 << CS21);
    TCNT2 = 0;
    TIMSK2 = (1 << TOIE2);
    sei();
}

ISR(TIMER2_OVF_vect) {
    spindle_encoder_handler();
}

void timer3_init(uint16_t ms) {
    cli();
    TCCR3A = 0;
    TCCR3B = (1 << WGM32) | (1 << CS31) | (1 << CS30);
    OCR3A = ms * 250UL;
    TIMSK3 = (1 << OCIE3A);
    sei();
}

ISR(TIMER3_COMPA_vect) {
    wdt_reset();
}

void timer4_init(uint16_t ms) {
    TCCR4A = 0;
    TCCR4B = (1 << WGM42) | (1 << CS41) | (1 << CS40);
    OCR4A = ms * 250UL;
    TIMSK4 = (1 << OCIE4A);
}

ISR(TIMER4_COMPA_vect) {
    ui_pot_read();
}
