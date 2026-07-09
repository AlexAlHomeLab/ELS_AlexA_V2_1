#include "../../config/config.h"
#include "debug_serial.h"

#include <Arduino.h>

static uint8_t serial_enabled = 1;

static const char *level_tag(uint8_t level) {
    switch (level) {
        case DEBUG_LEVEL_ERROR: return "ERROR";
        case DEBUG_LEVEL_WARNING: return "WARN";
        case DEBUG_LEVEL_VERBOSE: return "VERB";
        default: return "INFO";
    }
}

void debug_serial_init(uint32_t baud) {
    Serial.begin(baud);
    serial_enabled = 1;
}

void debug_serial_print(const char *msg) {
    if (!serial_enabled) return;
    Serial.print(msg);
}

void debug_serial_println(const char *msg) {
    if (!serial_enabled) return;
    Serial.println(msg);
}

void debug_serial_print_val(const char *label, uint32_t val) {
    if (!serial_enabled) return;
    Serial.print(label);
    Serial.print(": ");
    Serial.println(val);
}

void debug_serial_enable(uint8_t enable) {
    serial_enabled = enable;
}

void debug_log_event(uint8_t level, const char *module, const char *component, const char *msg) {
#if DEBUG_ENABLED
    if (!serial_enabled || level > DEBUG_LEVEL) return;
    Serial.print("[");
    Serial.print(level_tag(level));
    Serial.print("] [");
    Serial.print(module);
    Serial.print("] [");
    Serial.print(component);
    Serial.print("] ");
    Serial.println(msg);
#endif
}

void debug_log_event_val(uint8_t level, const char *module, const char *component, const char *msg, uint32_t val) {
#if DEBUG_ENABLED
    if (!serial_enabled || level > DEBUG_LEVEL) return;
    Serial.print("[");
    Serial.print(level_tag(level));
    Serial.print("] [");
    Serial.print(module);
    Serial.print("] [");
    Serial.print(component);
    Serial.print("] ");
    Serial.print(msg);
    Serial.println(val);
#endif
}
