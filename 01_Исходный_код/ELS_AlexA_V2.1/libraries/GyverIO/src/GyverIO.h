#pragma once
#include <Arduino.h>

namespace gio {
inline void init(uint8_t pin, uint8_t mode) { pinMode(pin, mode); }

inline bool read(uint8_t pin) {
    uint8_t port = digitalPinToPort(pin);
    if (port == NOT_A_PIN) return false;
    return (*portInputRegister(port) & digitalPinToBitMask(pin)) != 0;
}

inline void write(uint8_t pin, uint8_t level) { digitalWrite(pin, level); }
}
