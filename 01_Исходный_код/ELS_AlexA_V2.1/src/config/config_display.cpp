#include "config_display.h"
#include "../core/debug/debug_serial.h"
#include <Arduino.h>
#include <EEPROM.h>

#define EEPROM_DISPLAY_MAGIC_V1   0xD1   /* старый блок: только CrdU */
#define EEPROM_DISPLAY_MAGIC      0xD2   /* блок: CrdU + Xdia */
#define EEPROM_DISPLAY_ADDR       70     /* magic */
#define EEPROM_DISPLAY_ADDR_SUM   73     /* checksum V2 */
#define EEPROM_DISPLAY_ADDR_V1_SUM 72    /* checksum V1 (units only) */
#define EEPROM_DISPLAY_ADDR_OLD   64     /* очень старый адрес units */
#define EEPROM_DISPLAY_ADDR_SUM_OLD 66


static uint8_t coord_units = COORD_UNIT_DEFAULT;      /* COORD_UNIT_* */
static uint8_t x_coord_mode = X_COORD_MODE_DEFAULT;   /* X_COORD_MODE_* */

static uint8_t display_cfg_checksum_v2(uint8_t units, uint8_t xmode) {
    return (uint8_t)(EEPROM_DISPLAY_MAGIC + units + xmode);
}

static uint8_t display_cfg_checksum_v1(uint8_t units) {
    return (uint8_t)(EEPROM_DISPLAY_MAGIC_V1 + units);
}

static uint8_t display_cfg_validate_units(uint8_t units) {
    return units <= COORD_UNIT_INCH;
}

static uint8_t display_cfg_validate_xmode(uint8_t xmode) {
    return xmode <= X_COORD_MODE_DIAMETER;
}

/* Загрузка V2: magic, units, x_mode, sum. */
static uint8_t display_cfg_try_load_v2(void) {
    uint8_t magic = EEPROM.read(EEPROM_DISPLAY_ADDR);
    uint8_t units = EEPROM.read(EEPROM_DISPLAY_ADDR + 1);
    uint8_t xmode = EEPROM.read(EEPROM_DISPLAY_ADDR + 2);
    uint8_t sum_stored = EEPROM.read(EEPROM_DISPLAY_ADDR_SUM);

    if (magic == EEPROM_DISPLAY_MAGIC &&
        sum_stored == display_cfg_checksum_v2(units, xmode) &&
        display_cfg_validate_units(units) &&
        display_cfg_validate_xmode(xmode)) {
        coord_units = units;
        x_coord_mode = xmode;
        return 1U;
    }
    return 0U;
}

/* Миграция V1 → V2: сохранить CrdU, Xdia = default. */
static uint8_t display_cfg_try_load_v1(uint8_t addr, uint8_t sum_addr) {
    uint8_t magic = EEPROM.read(addr);
    uint8_t units = EEPROM.read((int)(addr + 1));
    uint8_t sum_stored = EEPROM.read(sum_addr);

    if (magic == EEPROM_DISPLAY_MAGIC_V1 &&
        sum_stored == display_cfg_checksum_v1(units) &&
        display_cfg_validate_units(units)) {
        coord_units = units;
        x_coord_mode = X_COORD_MODE_DEFAULT;
        return 1U;
    }
    return 0U;
}

void config_display_load(void) {
    if (display_cfg_try_load_v2()) {
        DBG_INFO("CFG", "DSP", "loaded");
        return;
    }
    if (display_cfg_try_load_v1(EEPROM_DISPLAY_ADDR, EEPROM_DISPLAY_ADDR_V1_SUM)) {
        config_display_save();
        DBG_INFO("CFG", "DSP", "migr v1");
        return;
    }
    if (display_cfg_try_load_v1(EEPROM_DISPLAY_ADDR_OLD, EEPROM_DISPLAY_ADDR_SUM_OLD)) {
        config_display_save();
        DBG_INFO("CFG", "DSP", "migr");
        return;
    }

    coord_units = COORD_UNIT_DEFAULT;
    x_coord_mode = X_COORD_MODE_DEFAULT;
    config_display_save();
    DBG_INFO("CFG", "DSP", "defaults");
}

void config_display_save(void) {
    if (!display_cfg_validate_units(coord_units)) {
        coord_units = COORD_UNIT_DEFAULT;
    }
    if (!display_cfg_validate_xmode(x_coord_mode)) {
        x_coord_mode = X_COORD_MODE_DEFAULT;
    }
    uint8_t sum = display_cfg_checksum_v2(coord_units, x_coord_mode);
    EEPROM.update(EEPROM_DISPLAY_ADDR, EEPROM_DISPLAY_MAGIC);
    EEPROM.update(EEPROM_DISPLAY_ADDR + 1, coord_units);
    EEPROM.update(EEPROM_DISPLAY_ADDR + 2, x_coord_mode);
    EEPROM.update(EEPROM_DISPLAY_ADDR_SUM, sum);
    DBG_INFO("CFG", "DSP", "saved");
}

void config_display_factory_reset(void) {
    coord_units = COORD_UNIT_DEFAULT;
    x_coord_mode = X_COORD_MODE_DEFAULT;
    config_display_save();
    DBG_INFO("CFG", "DSP", "factory");
}

uint8_t config_get_coord_units(void) {
    return coord_units;
}

void config_set_coord_units(uint8_t units) {
    if (units > COORD_UNIT_INCH) units = COORD_UNIT_INCH;
    coord_units = units;
}

char config_coord_unit_flag(void) {  /* символ единицы на LCD */
    switch (coord_units) {
    case COORD_UNIT_MM: return 'M';
    case COORD_UNIT_INCH: return 'D';
    default: return 'S';
    }
}

uint8_t config_get_x_coord_mode(void) {
    return x_coord_mode;
}

void config_set_x_coord_mode(uint8_t mode) {
    if (mode > X_COORD_MODE_DIAMETER) mode = X_COORD_MODE_DIAMETER;
    x_coord_mode = mode;
}

/* Буква оси X на главном экране / MPG: R или D. */
char config_x_coord_axis_char(void) {
    return (x_coord_mode == X_COORD_MODE_DIAMETER) ? 'D' : 'R';
}
