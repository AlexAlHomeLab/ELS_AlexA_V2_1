#include "config_feed.h"
#include "../core/debug/debug_serial.h"
#include <Arduino.h>
#include <EEPROM.h>

/* EEPROM feed block: magic@16, ranges@17, sum@25 */
#define EEPROM_FEED_MAGIC 0xB7
#define EEPROM_FEED_ADDR_MAGIC 16
#define EEPROM_FEED_ADDR_RANGES 17
#define EEPROM_FEED_ADDR_SUM 25

typedef struct {
    uint16_t min_raw;
    uint16_t max_raw;
} FeedRangeRaw_t;

static FeedRangeRaw_t feed_ranges[CONFIG_FEED_RANGE_COUNT];

static const FeedRangeRaw_t feed_defaults[CONFIG_FEED_RANGE_COUNT] = {
    {CONFIG_FEED_ASYNC_MIN_DEFAULT, CONFIG_FEED_ASYNC_MAX_DEFAULT},
    {CONFIG_FEED_SYNC_MIN_DEFAULT, CONFIG_FEED_SYNC_MAX_DEFAULT},
};

/* Async: все режимы кроме Sync и Thread; 0xFF — пот не используется */
static const uint8_t mode_feed_range[CONFIG_FEED_MODE_COUNT] = {
    FEED_RANGE_ASYNC,  /* Async */
    FEED_RANGE_SYNC,   /* Sync */
    0xFF,              /* Thread — пот не используется */
    FEED_RANGE_ASYNC,  /* Chamfer */
    FEED_RANGE_ASYNC,  /* Cone */
    FEED_RANGE_ASYNC,  /* Sphere */
    FEED_RANGE_ASYNC,  /* Divider */
    FEED_RANGE_ASYNC,  /* Grind */
};

static uint8_t feed_cfg_checksum(const FeedRangeRaw_t *ranges) {
    uint8_t sum = EEPROM_FEED_MAGIC;
    const uint8_t *p = (const uint8_t *)ranges;
    for (uint16_t i = 0; i < sizeof(FeedRangeRaw_t) * CONFIG_FEED_RANGE_COUNT; i++) {
        sum = (uint8_t)(sum + p[i]);
    }
    return sum;
}

static uint8_t feed_cfg_validate(const FeedRangeRaw_t *ranges) {
    for (uint8_t i = 0; i < CONFIG_FEED_RANGE_COUNT; i++) {
        if (ranges[i].min_raw > ranges[i].max_raw) return 0;
        if (ranges[i].max_raw == 0) return 0;
    }
    return 1;
}

static void feed_cfg_apply_defaults(void) {
    for (uint8_t i = 0; i < CONFIG_FEED_RANGE_COUNT; i++) {
        feed_ranges[i] = feed_defaults[i];
    }
}

static void feed_cfg_write_eeprom(void) {
    uint8_t sum = feed_cfg_checksum(feed_ranges);
    EEPROM.update(EEPROM_FEED_ADDR_MAGIC, EEPROM_FEED_MAGIC);
    const uint8_t *p = (const uint8_t *)feed_ranges;
    for (uint16_t i = 0; i < sizeof(FeedRangeRaw_t) * CONFIG_FEED_RANGE_COUNT; i++) {
        EEPROM.update((int)(EEPROM_FEED_ADDR_RANGES + i), p[i]);
    }
    EEPROM.update(EEPROM_FEED_ADDR_SUM, sum);
}

static uint16_t map_inverted_discrete(uint16_t min_raw, uint16_t max_raw, uint16_t adc) {
    /* пот инвертирован: max при ADC=0 */
    if (max_raw <= min_raw) return min_raw;
    return (uint16_t)(max_raw - (uint32_t)(max_raw - min_raw + 1) * adc / 1024U);
}

static float raw_to_value(FeedRangeId_t range_id, uint16_t raw) {
    if (range_id == FEED_RANGE_SYNC) {
        return (float)raw * CONFIG_FEED_SYNC_RAW_SCALE;
    }
    return (float)raw;
}

static FeedRangeId_t mode_to_range(uint8_t mode) {
    if (mode >= CONFIG_FEED_MODE_COUNT) mode = 0;
    uint8_t id = mode_feed_range[mode];
    if (id >= CONFIG_FEED_RANGE_COUNT) return FEED_RANGE_ASYNC;
    return (FeedRangeId_t)id;
}

void config_feed_load(void) {
    uint8_t magic = EEPROM.read(EEPROM_FEED_ADDR_MAGIC);
    uint8_t sum_stored = EEPROM.read(EEPROM_FEED_ADDR_SUM);
    FeedRangeRaw_t loaded[CONFIG_FEED_RANGE_COUNT];

    if (magic == EEPROM_FEED_MAGIC) {
        uint8_t *p = (uint8_t *)loaded;
        for (uint16_t i = 0; i < sizeof(FeedRangeRaw_t) * CONFIG_FEED_RANGE_COUNT; i++) {
            p[i] = EEPROM.read((int)(EEPROM_FEED_ADDR_RANGES + i));
        }
        if (sum_stored == feed_cfg_checksum(loaded) && feed_cfg_validate(loaded)) {
            for (uint8_t i = 0; i < CONFIG_FEED_RANGE_COUNT; i++) {
                feed_ranges[i] = loaded[i];
            }
            DBG_INFO("CFG", "FEED", "loaded");
            return;
        }
    }

    feed_cfg_apply_defaults();
    feed_cfg_write_eeprom();
    DBG_INFO("CFG", "FEED", "defaults");
}

void config_feed_save(void) {
    if (!feed_cfg_validate(feed_ranges)) {
        feed_cfg_apply_defaults();
    }
    feed_cfg_write_eeprom();
    DBG_INFO("CFG", "FEED", "saved");
}

uint16_t config_feed_get_min_raw(FeedRangeId_t range) {
    if (range >= CONFIG_FEED_RANGE_COUNT) range = FEED_RANGE_ASYNC;
    return feed_ranges[range].min_raw;
}

uint16_t config_feed_get_max_raw(FeedRangeId_t range) {
    if (range >= CONFIG_FEED_RANGE_COUNT) range = FEED_RANGE_ASYNC;
    return feed_ranges[range].max_raw;
}

void config_feed_set_range(FeedRangeId_t range, uint16_t min_raw, uint16_t max_raw) {
    if (range >= CONFIG_FEED_RANGE_COUNT) return;
    if (min_raw > max_raw) return;
    if (max_raw == 0) return;
    feed_ranges[range].min_raw = min_raw;
    feed_ranges[range].max_raw = max_raw;
}

uint8_t config_feed_uses_pot(uint8_t mode) {
    if (mode >= CONFIG_FEED_MODE_COUNT) mode = 0;
    return mode_feed_range[mode] != 0xFF;
}

FeedUnit_t config_feed_get_unit(uint8_t mode) {
    if (mode >= CONFIG_FEED_MODE_COUNT) mode = 0;
    if (mode == CONFIG_FEED_MODE_THREAD) return FEED_UNIT_NONE;
    if (mode == CONFIG_FEED_MODE_SYNC) return FEED_UNIT_MM_REV;
    return FEED_UNIT_MM_MIN;
}

float config_feed_get_min(uint8_t mode) {
    FeedRangeId_t id = mode_to_range(mode);
    return raw_to_value(id, feed_ranges[id].min_raw);
}

float config_feed_get_max(uint8_t mode) {
    FeedRangeId_t id = mode_to_range(mode);
    return raw_to_value(id, feed_ranges[id].max_raw);
}

float config_feed_map_adc(uint8_t mode, uint16_t adc) {  /* ADC 0..1023 → подача */
    if (!config_feed_uses_pot(mode)) return 0.0f;

    FeedRangeId_t id = mode_to_range(mode);
    const FeedRangeRaw_t *r = &feed_ranges[id];
    uint16_t raw = map_inverted_discrete(r->min_raw, r->max_raw, adc);

    if (id == FEED_RANGE_ASYNC && (r->max_raw - r->min_raw) >= CONFIG_FEED_ASYNC_QUANT_SPAN) {
        raw = (uint16_t)(r->min_raw + ((raw - r->min_raw) / CONFIG_FEED_ASYNC_QUANT_STEP) *
                         CONFIG_FEED_ASYNC_QUANT_STEP);
    }

    return raw_to_value(id, raw);
}
