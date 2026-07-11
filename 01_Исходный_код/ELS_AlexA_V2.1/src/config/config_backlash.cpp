#include "config_backlash.h"
#include "config_machine.h"
#include "config_defs.h"
#include "../core/debug/debug_serial.h"
#include <Arduino.h>
#include <EEPROM.h>

#define EEPROM_BACKLASH_MAGIC   0xB2
#define EEPROM_BACKLASH_ADDR    56
#define EEPROM_BACKLASH_ADDR_SUM 64

typedef struct __attribute__((packed)) {
    uint8_t auto_on;      /* автовыборка при старте */
    uint16_t steps_x;     /* шаги выборки X; 0 → CENTIMM */
    uint16_t steps_z;
    uint8_t auto_speed;   /* мм/мин */
    uint8_t min_speed;    /* мм/мин */
} BacklashCfg_t;

static BacklashCfg_t bl_cfg;

static const BacklashCfg_t bl_defaults = {
    BACKLASH_AUTO_DEFAULT,
    BACKLASH_X_STEPS_DEFAULT,
    BACKLASH_Z_STEPS_DEFAULT,
    (uint8_t)BACKLASH_AUTO_SPEED_DEFAULT,
    (uint8_t)BACKLASH_MIN_SPEED_DEFAULT
};

static void bl_cfg_normalize_speeds(void) {
    if (bl_cfg.auto_speed < BACKLASH_AUTO_SPEED_MIN) {
        bl_cfg.auto_speed = (uint8_t)BACKLASH_AUTO_SPEED_MIN;
    }
    if (bl_cfg.auto_speed > BACKLASH_AUTO_SPEED_MAX) {
        bl_cfg.auto_speed = (uint8_t)BACKLASH_AUTO_SPEED_MAX;
    }
    if (bl_cfg.min_speed < BACKLASH_MIN_SPEED_MIN) {
        bl_cfg.min_speed = (uint8_t)BACKLASH_MIN_SPEED_MIN;
    }
    if (bl_cfg.min_speed > BACKLASH_MIN_SPEED_MAX) {
        bl_cfg.min_speed = (uint8_t)BACKLASH_MIN_SPEED_MAX;
    }
    if (bl_cfg.min_speed > bl_cfg.auto_speed) {
        bl_cfg.min_speed = bl_cfg.auto_speed;
    }
}

static uint8_t bl_cfg_checksum(const BacklashCfg_t *c) {
    uint8_t sum = EEPROM_BACKLASH_MAGIC;
    const uint8_t *p = (const uint8_t *)c;
    for (uint8_t i = 0; i < sizeof(BacklashCfg_t); i++) {
        sum = (uint8_t)(sum + p[i]);
    }
    return sum;
}

static uint8_t bl_cfg_validate(const BacklashCfg_t *c) {
    if (c->auto_on > 1) return 0;
    if (c->steps_x > BACKLASH_STEPS_MAX) return 0;
    if (c->steps_z > BACKLASH_STEPS_MAX) return 0;
    if (c->auto_speed < BACKLASH_AUTO_SPEED_MIN ||
        c->auto_speed > BACKLASH_AUTO_SPEED_MAX) {
        return 0;
    }
    if (c->min_speed < BACKLASH_MIN_SPEED_MIN ||
        c->min_speed > BACKLASH_MIN_SPEED_MAX) {
        return 0;
    }
    if (c->min_speed > c->auto_speed) return 0;
    return 1;
}

static void bl_cfg_apply_defaults(void) {
    bl_cfg = bl_defaults;
}

static void bl_cfg_write_eeprom(void) {
    uint8_t sum = bl_cfg_checksum(&bl_cfg);
    EEPROM.update(EEPROM_BACKLASH_ADDR, EEPROM_BACKLASH_MAGIC);
    const uint8_t *p = (const uint8_t *)&bl_cfg;
    for (uint8_t i = 0; i < sizeof(BacklashCfg_t); i++) {
        EEPROM.update((int)(EEPROM_BACKLASH_ADDR + 1 + i), p[i]);
    }
    EEPROM.update(EEPROM_BACKLASH_ADDR_SUM, sum);
}

void config_backlash_load(void) {
    uint8_t magic = EEPROM.read(EEPROM_BACKLASH_ADDR);
    uint8_t sum_stored = EEPROM.read(EEPROM_BACKLASH_ADDR_SUM);
    BacklashCfg_t loaded;

    if (magic == EEPROM_BACKLASH_MAGIC) {
        uint8_t *p = (uint8_t *)&loaded;
        for (uint8_t i = 0; i < sizeof(BacklashCfg_t); i++) {
            p[i] = EEPROM.read((int)(EEPROM_BACKLASH_ADDR + 1 + i));
        }
        if (sum_stored == bl_cfg_checksum(&loaded) && bl_cfg_validate(&loaded)) {
            bl_cfg = loaded;
            DBG_INFO("CFG", "BL", "loaded");
            return;
        }
    }

    bl_cfg_apply_defaults();
    bl_cfg_write_eeprom();
    DBG_INFO("CFG", "BL", "defaults");
}

void config_backlash_save(void) {
    bl_cfg_normalize_speeds();
    if (!bl_cfg_validate(&bl_cfg)) {
        bl_cfg_apply_defaults();
    }
    bl_cfg_write_eeprom();
    DBG_INFO("CFG", "BL", "saved");
}

void config_backlash_factory_reset(void) {
    bl_cfg_apply_defaults();
    bl_cfg_write_eeprom();
    DBG_INFO("CFG", "BL", "factory");
}

uint8_t config_backlash_get_auto_on(void) {
    return bl_cfg.auto_on;
}

uint16_t config_backlash_get_steps_x(void) {
    return bl_cfg.steps_x;
}

uint16_t config_backlash_get_steps_z(void) {
    return bl_cfg.steps_z;
}

uint16_t config_backlash_get_auto_speed(void) {
    return bl_cfg.auto_speed;
}

uint16_t config_backlash_get_min_speed(void) {
    return bl_cfg.min_speed;
}

void config_backlash_set_auto_on(uint8_t on) {
    bl_cfg.auto_on = on ? 1U : 0U;
}

void config_backlash_set_steps_x(uint16_t steps) {
    if (steps > BACKLASH_STEPS_MAX) steps = BACKLASH_STEPS_MAX;
    bl_cfg.steps_x = steps;
}

void config_backlash_set_steps_z(uint16_t steps) {
    if (steps > BACKLASH_STEPS_MAX) steps = BACKLASH_STEPS_MAX;
    bl_cfg.steps_z = steps;
}

void config_backlash_set_auto_speed(uint16_t mm_min) {
    if (mm_min < BACKLASH_AUTO_SPEED_MIN) mm_min = BACKLASH_AUTO_SPEED_MIN;
    if (mm_min > BACKLASH_AUTO_SPEED_MAX) mm_min = BACKLASH_AUTO_SPEED_MAX;
    bl_cfg.auto_speed = (uint8_t)mm_min;
    bl_cfg_normalize_speeds();
}

void config_backlash_set_min_speed(uint16_t mm_min) {
    if (mm_min < BACKLASH_MIN_SPEED_MIN) mm_min = BACKLASH_MIN_SPEED_MIN;
    if (mm_min > BACKLASH_MIN_SPEED_MAX) mm_min = BACKLASH_MIN_SPEED_MAX;
    bl_cfg.min_speed = (uint8_t)mm_min;
    bl_cfg_normalize_speeds();
}

float config_backlash_comp_speed_mm_min(uint8_t axis, float feed_mm_min) {
    /* clamp к [min_speed, auto_speed] и max_speed оси */
    float min_s = (float)bl_cfg.min_speed;
    float max_s = (float)bl_cfg.auto_speed;
    float cap = (float)config_get_max_speed_mm_min(axis);

    if (feed_mm_min < min_s) feed_mm_min = min_s;
    if (feed_mm_min > max_s) feed_mm_min = max_s;
    if (feed_mm_min < 1.0f) feed_mm_min = 1.0f;
    if (feed_mm_min > cap) feed_mm_min = cap;
    return feed_mm_min;
}

float config_backlash_runtime_speed_mm_min(uint8_t axis, float feed_mm_min) {
    float min_s = (float)bl_cfg.min_speed;
    float cap = (float)config_get_max_speed_mm_min(axis);

    if (feed_mm_min < min_s) feed_mm_min = min_s;
    if (feed_mm_min < 1.0f) feed_mm_min = 1.0f;
    if (feed_mm_min > cap) feed_mm_min = cap;
    return feed_mm_min;
}
