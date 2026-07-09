#include "hal_ports.h"
#include <Arduino.h>
#include <avr/io.h>

uint8_t hal_pin_read(uint8_t pin) {
    uint8_t port = digitalPinToPort(pin);
    if (port == NOT_A_PIN) return 1;
    return (*portInputRegister(port) & digitalPinToBitMask(pin)) ? 1 : 0;
}

void port_set(uint8_t port, uint8_t pin) {
    switch (port) {
        case PORT_B: PORTB |= (1 << pin); break;
        case PORT_C: PORTC |= (1 << pin); break;
        case PORT_D: PORTD |= (1 << pin); break;
        case PORT_E: PORTE |= (1 << pin); break;
        case PORT_F: PORTF |= (1 << pin); break;
        case PORT_G: PORTG |= (1 << pin); break;
        case PORT_H: PORTH |= (1 << pin); break;
        case PORT_J: PORTJ |= (1 << pin); break;
        case PORT_K: PORTK |= (1 << pin); break;
        case PORT_L: PORTL |= (1 << pin); break;
        default: break;
    }
}

void port_clear(uint8_t port, uint8_t pin) {
    switch (port) {
        case PORT_B: PORTB &= ~(1 << pin); break;
        case PORT_C: PORTC &= ~(1 << pin); break;
        case PORT_D: PORTD &= ~(1 << pin); break;
        case PORT_E: PORTE &= ~(1 << pin); break;
        case PORT_F: PORTF &= ~(1 << pin); break;
        case PORT_G: PORTG &= ~(1 << pin); break;
        case PORT_H: PORTH &= ~(1 << pin); break;
        case PORT_J: PORTJ &= ~(1 << pin); break;
        case PORT_K: PORTK &= ~(1 << pin); break;
        case PORT_L: PORTL &= ~(1 << pin); break;
        default: break;
    }
}

uint8_t port_read(uint8_t port, uint8_t pin) {
    switch (port) {
        case PORT_B: return (PINB & (1 << pin)) ? 1 : 0;
        case PORT_C: return (PINC & (1 << pin)) ? 1 : 0;
        case PORT_D: return (PIND & (1 << pin)) ? 1 : 0;
        case PORT_E: return (PINE & (1 << pin)) ? 1 : 0;
        case PORT_F: return (PINF & (1 << pin)) ? 1 : 0;
        case PORT_G: return (PING & (1 << pin)) ? 1 : 0;
        case PORT_H: return (PINH & (1 << pin)) ? 1 : 0;
        case PORT_J: return (PINJ & (1 << pin)) ? 1 : 0;
        case PORT_K: return (PINK & (1 << pin)) ? 1 : 0;
        case PORT_L: return (PINL & (1 << pin)) ? 1 : 0;
        default: return 0;
    }
}

void port_toggle(uint8_t port, uint8_t pin) {
    if (port_read(port, pin)) {
        port_clear(port, pin);
    } else {
        port_set(port, pin);
    }
}
