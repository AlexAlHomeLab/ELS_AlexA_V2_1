#pragma once
#include <Arduino.h>

namespace gio {
inline void init(uint8_t pin, uint8_t mode) { pinMode(pin, mode); }
inline bool read(uint8_t pin) { return digitalRead(pin); }
inline void write(uint8_t pin, uint8_t level) { digitalWrite(pin, level); }
}
