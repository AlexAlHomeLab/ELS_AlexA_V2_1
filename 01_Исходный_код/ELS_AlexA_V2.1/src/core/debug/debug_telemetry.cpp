#include "debug_telemetry.h"
#include "../../config/config.h"
#include <Arduino.h>
#include <string.h>

static TelemetryEntry_t buffer[TELEMETRY_BUFFER_SIZE];
static uint8_t head = 0;
static uint8_t count = 0;

void debug_telemetry_init(void) {
    memset(buffer, 0, sizeof(buffer));
    head = 0;
    count = 0;
}

void debug_telemetry_log(uint8_t level, const char *module,
                         const char *component, const char *msg, uint32_t val) {
    if (level > DEBUG_LEVEL) return;

    TelemetryEntry_t *entry = &buffer[head];
    entry->timestamp = millis();
    entry->level = level;
    entry->module = module;
    entry->component = component;
    entry->message = msg;
    entry->value = val;

    head = (head + 1) % TELEMETRY_BUFFER_SIZE;
    if (count < TELEMETRY_BUFFER_SIZE) count++;
}

void debug_telemetry_send(void) {
    uint8_t idx = (head + TELEMETRY_BUFFER_SIZE - count) % TELEMETRY_BUFFER_SIZE;
    for (uint8_t i = 0; i < count; i++) {
        TelemetryEntry_t *e = &buffer[idx];
        Serial.print("[");
        Serial.print(e->timestamp);
        Serial.print("] [");
        Serial.print(e->level);
        Serial.print("] [");
        Serial.print(e->module);
        Serial.print("] [");
        Serial.print(e->component);
        Serial.print("] ");
        Serial.print(e->message);
        if (e->value) {
            Serial.print(" ");
            Serial.print(e->value);
        }
        Serial.println();
        idx = (idx + 1) % TELEMETRY_BUFFER_SIZE;
    }
}

void debug_telemetry_clear(void) {
    head = 0;
    count = 0;
}
