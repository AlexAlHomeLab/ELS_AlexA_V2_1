#include "ui_pot.h"
#include "ui_switches.h"
#include "../fsm/fsm_manager.h"
#include "../process/spindle_control.h"
#include "../hal/hal_pins.h"
#include "../../config/config.h"
#include "../../config/config_feed.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>

#define POT_FILTER_LEN 16
#define POT_FILTER_THRESH 4

static uint16_t adc_array[POT_FILTER_LEN];
static uint16_t adc_sum = 0;
static uint8_t adc_idx = 0;
static uint16_t pot_filtered = 0;
static volatile uint8_t pot_changed = 0;
static float last_feed_value = -1.0f;

static void pot_filter_reset(uint16_t val) {
    for (uint8_t i = 0; i < POT_FILTER_LEN; i++) {
        adc_array[i] = val;
    }
    adc_sum = (uint16_t)(val * POT_FILTER_LEN);
    pot_filtered = val;
}

static void pot_filter_push(uint16_t val) {
    adc_sum = (uint16_t)(adc_sum - adc_array[adc_idx]);
    adc_array[adc_idx] = val;
    adc_sum = (uint16_t)(adc_sum + val);
    adc_idx = (uint8_t)((adc_idx + 1U) % POT_FILTER_LEN);
    pot_filtered = (uint16_t)(adc_sum / POT_FILTER_LEN);
}

void ui_pot_init(void) {
    pinMode(POT_FEED_PIN, INPUT);
    uint16_t val = (uint16_t)analogRead(POT_FEED_PIN);
    pot_filter_reset(val);
    pot_changed = 0;
    last_feed_value = -1.0f;
}

void ui_pot_read(void) {
    int val = analogRead(POT_FEED_PIN);
    if (val > (int)pot_filtered + POT_FILTER_THRESH ||
        val < (int)pot_filtered - POT_FILTER_THRESH) {
        pot_filter_push((uint16_t)val);
        pot_changed = 1;
    }
}

uint16_t ui_pot_get_value(void) {
    return pot_filtered;
}

uint8_t ui_pot_get_percent(void) {
    return (uint8_t)((pot_filtered * 100UL) / 1024UL);
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

float ui_pot_get_feed_value(uint8_t mode) {
    if (mode >= CONFIG_FEED_MODE_COUNT) mode = 0;
    if (!config_feed_uses_pot(mode)) return 0.0f;
    return config_feed_map_adc(mode, pot_filtered);
}

float ui_pot_get_jog_mm_min(void) {
    uint8_t mode = fsm_manager_get_mode();
    FeedUnit_t unit = config_feed_get_unit(mode);

    if (unit == FEED_UNIT_NONE) {
        return config_feed_map_adc(CONFIG_FEED_MODE_ASYNC, pot_filtered);
    }

    float value = ui_pot_get_feed_value(mode);
    if (unit == FEED_UNIT_MM_MIN) {
        return value;
    }
    if (unit == FEED_UNIT_MM_REV) {
        uint32_t rpm = spindle_get_rpm();
        if (rpm < 10U) rpm = 60U;
        return value * (float)rpm;
    }
    return config_feed_get_min(CONFIG_FEED_MODE_ASYNC);
}

void ui_pot_feed_format(char *buf, size_t len, uint8_t mode) {
    if (!buf || len == 0) return;
    if (mode >= CONFIG_FEED_MODE_COUNT) mode = 0;

    if (!config_feed_uses_pot(mode)) {
        snprintf(buf, len, "F:---");
        return;
    }

    FeedUnit_t unit = config_feed_get_unit(mode);
    float value = ui_pot_get_feed_value(mode);

    if (unit == FEED_UNIT_MM_MIN) {
        snprintf(buf, len, "F:%u", (unsigned)(value + 0.5f));
        return;
    }
    if (unit == FEED_UNIT_MM_REV) {
        uint16_t cents = (uint16_t)(value * 100.0f + 0.5f);
        snprintf(buf, len, "F:%u.%02u", (unsigned)(cents / 100U), (unsigned)(cents % 100U));
    }
}

uint8_t ui_pot_poll(void) {
    ui_pot_read();
    uint8_t mode = fsm_manager_get_mode();
    if (!config_feed_uses_pot(mode)) return 0;

    float feed = ui_pot_get_feed_value(mode);
    if (feed != last_feed_value) {
        last_feed_value = feed;
        return 1;
    }
    return 0;
}
