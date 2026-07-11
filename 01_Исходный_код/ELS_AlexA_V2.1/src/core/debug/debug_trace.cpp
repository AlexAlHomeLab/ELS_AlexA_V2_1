#include "debug_trace.h"

#if TRACE_CRASH_ENABLED

#include <Arduino.h>

/* Переживает soft reset; после ребута читается в trace_crash_init() */
volatile uint8_t trace_last_id __attribute__((section(".noinit")));
volatile uint8_t trace_isr_last_id __attribute__((section(".noinit")));

void trace_crash_init(void) {
    /* LST/ISR всегда при ребуте; >NN — только если TRACE_CRASH_SERIAL=1 */
    Serial.print(F("LST="));
    Serial.println(trace_last_id);
    Serial.print(F("ISR="));
    Serial.println(trace_isr_last_id);
    if (trace_isr_last_id == TR_ISR_AFTER_Z) {
        Serial.println(F("CRH: after Z ISR"));
    }
    trace_last_id = 0;
    trace_isr_last_id = 0;
}

void trace_crash_mark(uint8_t id) {
    trace_last_id = id;
#if TRACE_CRASH_SERIAL
    Serial.write('>');
    if (id >= 100U) {
        Serial.write((char)('0' + (id / 100U) % 10U));
    }
    if (id >= 10U) {
        Serial.write((char)('0' + (id / 10U) % 10U));
    }
    Serial.write((char)('0' + id % 10U));
    Serial.write('\n');
#endif
}

void trace_crash_mark_isr(uint8_t id) {
    trace_isr_last_id = id;
}

#endif
