#include "config_display.h"
#include "../core/debug/debug_serial.h"
#include <Arduino.h>
#include <EEPROM.h>

#define EEPROM_DISPLAY_MAGIC      0xD1   /* сигнатура блока CrdU / display */
#define EEPROM_DISPLAY_ADDR       70     /* адрес units (COORD_UNIT_*) */
#define EEPROM_DISPLAY_ADDR_SUM   72     /* адрес checksum */
#define EEPROM_DISPLAY_ADDR_OLD   64     /* старый адрес units (миграция) */
#define EEPROM_DISPLAY_ADDR_SUM_OLD 66   /* старый адрес checksum */


static uint8_t coord_units = COORD_UNIT_DEFAULT;  /* COORD_UNIT_* */

static uint8_t display_cfg_checksum(uint8_t units) {
    return (uint8_t)(EEPROM_DISPLAY_MAGIC + units);
}

static uint8_t display_cfg_validate(uint8_t units) {
    return units <= COORD_UNIT_INCH;
}

static uint8_t display_cfg_try_load(uint8_t addr, uint8_t sum_addr) {
    uint8_t magic = EEPROM.read(addr);
    uint8_t units = EEPROM.read((int)(addr + 1));
    uint8_t sum_stored = EEPROM.read(sum_addr);

    if (magic == EEPROM_DISPLAY_MAGIC &&
        sum_stored == display_cfg_checksum(units) &&
        display_cfg_validate(units)) {
        coord_units = units;
        return 1U;
    }
    return 0U;
}

void config_display_load(void) {
    if (display_cfg_try_load(EEPROM_DISPLAY_ADDR, EEPROM_DISPLAY_ADDR_SUM)) {
        DBG_INFO("CFG", "DSP", "loaded");
        return;
    }
    if (display_cfg_try_load(EEPROM_DISPLAY_ADDR_OLD, EEPROM_DISPLAY_ADDR_SUM_OLD)) {
        config_display_save();
        DBG_INFO("CFG", "DSP", "migr");
        return;
    }

    coord_units = COORD_UNIT_DEFAULT;
    config_display_save();
    DBG_INFO("CFG", "DSP", "defaults");
}

void config_display_save(void) {
    if (!display_cfg_validate(coord_units)) {
        coord_units = COORD_UNIT_DEFAULT;
    }
    uint8_t sum = display_cfg_checksum(coord_units);
    EEPROM.update(EEPROM_DISPLAY_ADDR, EEPROM_DISPLAY_MAGIC);
    EEPROM.update(EEPROM_DISPLAY_ADDR + 1, coord_units);
    EEPROM.update(EEPROM_DISPLAY_ADDR_SUM, sum);
    DBG_INFO("CFG", "DSP", "saved");
}

void config_display_factory_reset(void) {
    coord_units = COORD_UNIT_DEFAULT;
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
