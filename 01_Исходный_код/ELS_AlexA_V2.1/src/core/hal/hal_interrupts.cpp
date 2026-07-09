#include "hal_interrupts.h"
#include "../process/spindle_control.h"
#include "../ui/ui_encoder.h"
#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>

void encoder_interrupts_init(void) {
}

void mpg_interrupts_init(void) {
}

void buttons_interrupts_init(void) {
    PCICR |= (1 << PCIE2);
    PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
}

void buttons_scan(void) {
}

ISR(INT3_vect) {
    spindle_encoder_process((PIND >> PD0) & 1, (PIND >> PD1) & 1);
}

ISR(INT4_vect) {
    spindle_encoder_process((PIND >> PD0) & 1, (PIND >> PD1) & 1);
}

ISR(INT5_vect) {
    mpg_process((PINF >> PF4) & 1, (PINF >> PF5) & 1);
}

ISR(INT6_vect) {
    mpg_process((PINF >> PF4) & 1, (PINF >> PF5) & 1);
}

ISR(PCINT2_vect) {
    buttons_scan();
}
