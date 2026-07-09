#include "ui_pot.h"
#include "ui_switches.h"
#include "../hal/hal_pins.h"
#include "../../config/config.h"
#include <Arduino.h>
#include <stdlib.h>

static volatile uint16_t pot_value = 0;
static volatile uint8_t pot_changed = 0;
static uint8_t last_percent = 0;

void ui_pot_init(void) {
    pinMode(POT_FEED_PIN, INPUT);
    ui_pot_read();
    last_percent = ui_pot_get_percent();
}

void ui_pot_read(void) {
    uint16_t val = analogRead(POT_FEED_PIN);
    if (abs((int)val - (int)pot_value) > 5) {
        pot_value = val;
        pot_changed = 1;
    }
}

uint16_t ui_pot_get_value(void) {
    return pot_value;
}

uint8_t ui_pot_get_percent(void) {
    return (uint8_t)((pot_value * 100UL) / 1024UL);
}

uint8_t ui_pot_has_changed(void) {
    uint8_t ret = pot_changed;
    pot_changed = 0;
    return ret;
}

uint8_t ui_pot_get_mode(void) {
    return ui_switches_get_state().mode;
}

uint8_t ui_pot_get_submode(void) {
    return ui_switches_get_state().submode;
}

uint8_t ui_pot_poll(void) {
    ui_pot_read();
    uint8_t pct = ui_pot_get_percent();
    if (pct != last_percent) {
        last_percent = pct;
        return 1;
    }
    return 0;
}
