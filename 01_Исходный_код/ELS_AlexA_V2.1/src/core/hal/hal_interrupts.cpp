#include "hal_interrupts.h"
#include "../process/spindle_control.h"
#include "../ui/ui_encoder.h"
#include "../../config/config_machine.h"
#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>

static void mpg_interrupts_init(void) {
    EICRA |= (1 << ISC20);
    EIFR = (1 << INTF2);
    EIMSK |= (1 << INT2);
}

/* INT0 (D21 / канал B): только Rising — ×1 к PPR, вдвое меньше ISR чем Any Change.
 * ISC01:ISC00 = 11; биты сбрасываем явно, чтобы не осталось Low/Change. */
void encoder_interrupts_init(void) {
#if ENABLE_SPINDLE_ENCODER
    EICRA = (uint8_t)((EICRA & (uint8_t)~((1 << ISC01) | (1 << ISC00)))
                      | (1 << ISC01) | (1 << ISC00));
    EIFR = (1 << INTF0);
    EIMSK |= (1 << INT0);
#endif
    mpg_interrupts_init();
}

void mpg_int_enable(void) {
    EIFR = (1 << INTF2);
    EIMSK |= (1 << INT2);
}

void mpg_int_disable(void) {
    EIMSK &= ~(1 << INT2);
}

#if ENABLE_SPINDLE_ENCODER
ISR(INT0_vect) {
    spindle_encoder_isr_step();
}
#endif

ISR(INT2_vect) {
    hand_mpg_isr_step();
}
