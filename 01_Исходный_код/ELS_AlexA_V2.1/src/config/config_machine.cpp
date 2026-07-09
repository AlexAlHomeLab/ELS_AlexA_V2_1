#include "config_machine.h"
#include "../core/debug/debug_serial.h"
#include <Arduino.h>
#include <EEPROM.h>

#define EEPROM_MACHINE_MAGIC 0xC5
#define EEPROM_MACHINE_ADDR_MAGIC 32
#define EEPROM_MACHINE_ADDR_DATA 33
#define EEPROM_MACHINE_ADDR_SUM 55

typedef struct {
    uint16_t motor_steps;
    uint8_t microstep;
    uint16_t screw_pitch;
    uint16_t max_speed;
    uint16_t rapid_speed;
    uint8_t feed_accel;
    uint8_t dir_invert;
} AxisCfgRaw_t;

typedef struct {
    AxisCfgRaw_t ax[2];
    uint16_t spindle_ppr;
} MachineCfg_t;

static MachineCfg_t machine_cfg;

static const MachineCfg_t machine_defaults = {
    {{400, 2, 42, 400, 1500, 3, AXIS_Z_DIR_INVERT_DEFAULT},
     {200, 4, 160, 400, 2000, 3, AXIS_X_DIR_INVERT_DEFAULT}},
    3000
};

static AxisCfgRaw_t *axis_cfg(uint8_t axis) {
    if (axis > AXIS_Z) axis = AXIS_Z;
    return &machine_cfg.ax[axis];
}

static uint8_t axis_cfg_validate(const AxisCfgRaw_t *a) {
    if (a->motor_steps < 50 || a->motor_steps > 2000) return 0;
    if (a->microstep < 1 || a->microstep > 32) return 0;
    if (a->screw_pitch < 10 || a->screw_pitch > 1000) return 0;
    if (a->max_speed < 10 || a->max_speed > 5000) return 0;
    if (a->rapid_speed < 10 || a->rapid_speed > 10000) return 0;
    if (a->feed_accel < 1 || a->feed_accel > 20) return 0;
    if (a->dir_invert > 1) return 0;
    return 1;
}

static uint8_t machine_cfg_checksum(const MachineCfg_t *cfg) {
    uint8_t sum = EEPROM_MACHINE_MAGIC;
    const uint8_t *p = (const uint8_t *)cfg;
    for (uint16_t i = 0; i < sizeof(MachineCfg_t); i++) {
        sum = (uint8_t)(sum + p[i]);
    }
    return sum;
}

static uint8_t machine_cfg_validate(const MachineCfg_t *cfg) {
    if (!axis_cfg_validate(&cfg->ax[AXIS_X])) return 0;
    if (!axis_cfg_validate(&cfg->ax[AXIS_Z])) return 0;
    if (cfg->spindle_ppr < 100 || cfg->spindle_ppr > 10000) return 0;
    return 1;
}

static void machine_cfg_apply_defaults(void) {
    machine_cfg = machine_defaults;
}

static void machine_cfg_write_eeprom(void) {
    uint8_t sum = machine_cfg_checksum(&machine_cfg);
    EEPROM.update(EEPROM_MACHINE_ADDR_MAGIC, EEPROM_MACHINE_MAGIC);
    const uint8_t *p = (const uint8_t *)&machine_cfg;
    for (uint16_t i = 0; i < sizeof(MachineCfg_t); i++) {
        EEPROM.update((int)(EEPROM_MACHINE_ADDR_DATA + i), p[i]);
    }
    EEPROM.update(EEPROM_MACHINE_ADDR_SUM, sum);
}

void config_machine_load(void) {
    uint8_t magic = EEPROM.read(EEPROM_MACHINE_ADDR_MAGIC);
    uint8_t sum_stored = EEPROM.read(EEPROM_MACHINE_ADDR_SUM);
    MachineCfg_t loaded;

    if (magic == EEPROM_MACHINE_MAGIC) {
        uint8_t *p = (uint8_t *)&loaded;
        for (uint16_t i = 0; i < sizeof(MachineCfg_t); i++) {
            p[i] = EEPROM.read((int)(EEPROM_MACHINE_ADDR_DATA + i));
        }
        if (sum_stored == machine_cfg_checksum(&loaded) && machine_cfg_validate(&loaded)) {
            machine_cfg = loaded;
            DBG_INFO("CFG", "MACH", "loaded");
            return;
        }
    }

    machine_cfg_apply_defaults();
    machine_cfg_write_eeprom();
    DBG_INFO("CFG", "MACH", "defaults");
}

void config_machine_save(void) {
    if (!machine_cfg_validate(&machine_cfg)) {
        machine_cfg_apply_defaults();
    }
    machine_cfg_write_eeprom();
    DBG_INFO("CFG", "MACH", "saved");
}

float config_get_steps_per_mm(uint8_t axis) {
    const AxisCfgRaw_t *a = axis_cfg(axis);
    return (float)a->motor_steps * (float)a->microstep / ((float)a->screw_pitch * 0.01f);
}

uint16_t config_get_motor_steps(uint8_t axis) {
    return axis_cfg(axis)->motor_steps;
}

uint8_t config_get_microstep(uint8_t axis) {
    return axis_cfg(axis)->microstep;
}

uint16_t config_get_screw_pitch(uint8_t axis) {
    return axis_cfg(axis)->screw_pitch;
}

uint16_t config_get_max_speed_mm_min(uint8_t axis) {
    return axis_cfg(axis)->max_speed;
}

uint16_t config_get_rapid_speed_mm_min(uint8_t axis) {
    return axis_cfg(axis)->rapid_speed;
}

uint8_t config_get_feed_accel(uint8_t axis) {
    return axis_cfg(axis)->feed_accel;
}

uint8_t config_get_dir_invert(uint8_t axis) {
    return axis_cfg(axis)->dir_invert;
}

uint16_t config_get_spindle_ppr(void) {
    return machine_cfg.spindle_ppr;
}

uint32_t config_mm_min_to_sps(uint8_t axis, float mm_min) {
    if (mm_min < 1.0f) mm_min = 1.0f;
    float spm = config_get_steps_per_mm(axis);
    uint32_t sps = (uint32_t)((mm_min * spm) / 60.0f);
    if (sps < 1U) sps = 1U;
    return sps;
}

void config_set_motor_steps(uint8_t axis, uint16_t steps) {
    if (steps < 50 || steps > 2000) return;
    axis_cfg(axis)->motor_steps = steps;
}

void config_set_microstep(uint8_t axis, uint8_t ms) {
    if (ms < 1 || ms > 32) return;
    axis_cfg(axis)->microstep = ms;
}

void config_set_screw_pitch(uint8_t axis, uint16_t pitch_cents) {
    if (pitch_cents < 10 || pitch_cents > 1000) return;
    axis_cfg(axis)->screw_pitch = pitch_cents;
}

void config_set_max_speed_mm_min(uint8_t axis, uint16_t mm_min) {
    if (mm_min < 10 || mm_min > 5000) return;
    axis_cfg(axis)->max_speed = mm_min;
}

void config_set_rapid_speed_mm_min(uint8_t axis, uint16_t mm_min) {
    if (mm_min < 10 || mm_min > 10000) return;
    axis_cfg(axis)->rapid_speed = mm_min;
}

void config_set_feed_accel(uint8_t axis, uint8_t accel) {
    if (accel < 1 || accel > 20) return;
    axis_cfg(axis)->feed_accel = accel;
}

void config_set_dir_invert(uint8_t axis, uint8_t invert) {
    axis_cfg(axis)->dir_invert = invert ? 1U : 0U;
}

void config_set_spindle_ppr(uint16_t ppr) {
    if (ppr < 100 || ppr > 10000) return;
    machine_cfg.spindle_ppr = ppr;
}
