#include "config_storage.h"
#include "config_feed.h"
#include "config_machine.h"
#include "../core/debug/debug_serial.h"
#include <Arduino.h>
#include <EEPROM.h>

#define EEPROM_ADDR_MAGIC 0
#define EEPROM_ADDR_FEED 1
#define EEPROM_ADDR_BUZZER 2
#define EEPROM_ADDR_CHECKSUM 3

static uint8_t cfg_feed_max = CONFIG_FEED_MAX_DEFAULT;
static uint8_t cfg_buzzer_on = CONFIG_BUZZER_DEFAULT;

static uint8_t config_checksum(uint8_t magic, uint8_t feed, uint8_t buzzer) {
    return (uint8_t)(magic + feed + buzzer);
}

static void config_read_raw(uint8_t *magic, uint8_t *feed, uint8_t *buzzer, uint8_t *checksum) {
    *magic = EEPROM.read(EEPROM_ADDR_MAGIC);
    *feed = EEPROM.read(EEPROM_ADDR_FEED);
    *buzzer = EEPROM.read(EEPROM_ADDR_BUZZER);
    *checksum = EEPROM.read(EEPROM_ADDR_CHECKSUM);
}

static uint8_t config_validate_raw(uint8_t magic, uint8_t feed, uint8_t buzzer, uint8_t checksum) {
    if (magic != CONFIG_EEPROM_MAGIC) return 0;
    if (feed < 10 || feed > 100) return 0;
    if (buzzer > 1) return 0;
    return checksum == config_checksum(magic, feed, buzzer);
}

static void config_apply(uint8_t feed, uint8_t buzzer) {
    cfg_feed_max = feed;
    cfg_buzzer_on = buzzer;
}

static void config_write_eeprom(void) {
    uint8_t sum = config_checksum(CONFIG_EEPROM_MAGIC, cfg_feed_max, cfg_buzzer_on);
    EEPROM.update(EEPROM_ADDR_MAGIC, CONFIG_EEPROM_MAGIC);
    EEPROM.update(EEPROM_ADDR_FEED, cfg_feed_max);
    EEPROM.update(EEPROM_ADDR_BUZZER, cfg_buzzer_on);
    EEPROM.update(EEPROM_ADDR_CHECKSUM, sum);
}

void config_load(void) {
    uint8_t magic, feed, buzzer, checksum;
    config_read_raw(&magic, &feed, &buzzer, &checksum);

    if (config_validate_raw(magic, feed, buzzer, checksum)) {
        config_apply(feed, buzzer);
        DBG_INFO_VAL("CFG", "LOAD", "feed", cfg_feed_max);
        DBG_INFO_VAL("CFG", "LOAD", "buzz", cfg_buzzer_on);
    } else {
        config_apply(CONFIG_FEED_MAX_DEFAULT, CONFIG_BUZZER_DEFAULT);

        if (magic != CONFIG_EEPROM_MAGIC) {
            config_write_eeprom();
            DBG_INFO("CFG", "LOAD", "defaults new");
        } else {
            DBG_INFO_VAL("CFG", "LOAD", "bad raw", magic);
            DBG_INFO_VAL("CFG", "LOAD", "feed", feed);
            DBG_INFO_VAL("CFG", "LOAD", "buzz", buzzer);
            DBG_INFO_VAL("CFG", "LOAD", "sum", checksum);
        }
    }

    config_feed_load();
    config_machine_load();
    config_backlash_load();
    config_display_load();
}

void config_save(void) {
    config_write_eeprom();

    uint8_t magic, feed, buzzer, checksum;
    config_read_raw(&magic, &feed, &buzzer, &checksum);
    if (config_validate_raw(magic, feed, buzzer, checksum)) {
        DBG_INFO_VAL("CFG", "SAVE", "feed", cfg_feed_max);
        DBG_INFO_VAL("CFG", "SAVE", "buzz", cfg_buzzer_on);
    } else {
        DBG_INFO("CFG", "SAVE", "verify fail");
    }
}

uint8_t config_get_feed_max(void) {
    return cfg_feed_max;
}

uint8_t config_get_buzzer_on(void) {
    return cfg_buzzer_on;
}

void config_set_feed_max(uint8_t pct) {
    if (pct < 10) pct = 10;
    if (pct > 100) pct = 100;
    cfg_feed_max = pct;
}

void config_set_buzzer_on(uint8_t on) {
    cfg_buzzer_on = on ? 1 : 0;
}
