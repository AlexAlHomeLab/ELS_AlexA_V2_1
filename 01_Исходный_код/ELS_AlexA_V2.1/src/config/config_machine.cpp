#include "config_machine.h"
#include "../core/debug/debug_serial.h"
#include <Arduino.h>
#include <EEPROM.h>

/* EEPROM machine block: magic@32, data@33, sum@55 */
#define EEPROM_MACHINE_MAGIC 0xC5
#define EEPROM_MACHINE_ADDR_MAGIC 32
#define EEPROM_MACHINE_ADDR_DATA 33
#define EEPROM_MACHINE_ADDR_SUM 55

/* Сырые параметры одной оси в EEPROM */
typedef struct {
    uint16_t motor_steps;   /* полных шагов двигателя */
    uint8_t microstep;      /* делитель */
    uint16_t screw_pitch;   /* шаг ВПК, ×100 мм */
    uint16_t max_speed;     /* мм/мин */
    uint16_t rapid_speed;   /* мм/мин */
    uint8_t feed_accel;     /* уровень ускорения */
    uint8_t dir_invert;     /* 0/1 */
} AxisCfgRaw_t;

typedef struct {
    AxisCfgRaw_t ax[2];     /* X, Z */
    uint16_t spindle_ppr;
    uint16_t jog_decel_steps;
} MachineCfg_t;

static MachineCfg_t machine_cfg;

static const MachineCfg_t machine_defaults = {
    {{AXIS_X_MOTOR_STEPS_DEFAULT, AXIS_X_MICROSTEP_DEFAULT, AXIS_X_SCREW_PITCH_DEFAULT,
      AXIS_X_MAX_SPEED_DEFAULT, AXIS_X_RAPID_SPEED_DEFAULT, AXIS_X_FEED_ACCEL_DEFAULT,
      AXIS_X_DIR_INVERT_DEFAULT},
     {AXIS_Z_MOTOR_STEPS_DEFAULT, AXIS_Z_MICROSTEP_DEFAULT, AXIS_Z_SCREW_PITCH_DEFAULT,
      AXIS_Z_MAX_SPEED_DEFAULT, AXIS_Z_RAPID_SPEED_DEFAULT, AXIS_Z_FEED_ACCEL_DEFAULT,
      AXIS_Z_DIR_INVERT_DEFAULT}},
    SPINDLE_PPR_DEFAULT,
    JOG_DECEL_STEPS_DEFAULT
};

static AxisCfgRaw_t *axis_cfg(uint8_t axis) {  /* указатель на ax[X|Z] */
    if (axis > AXIS_Z) axis = AXIS_Z;
    return &machine_cfg.ax[axis];
}

static uint8_t axis_cfg_validate(const AxisCfgRaw_t *a) {
    if (a->motor_steps < AXIS_MOTOR_STEPS_MIN || a->motor_steps > AXIS_MOTOR_STEPS_MAX) return 0;
    if (a->microstep < AXIS_MICROSTEP_MIN || a->microstep > AXIS_MICROSTEP_MAX) return 0;
    if (a->screw_pitch < AXIS_SCREW_PITCH_MIN || a->screw_pitch > AXIS_SCREW_PITCH_MAX) return 0;
    if (a->max_speed < AXIS_MAX_SPEED_MIN || a->max_speed > AXIS_MAX_SPEED_MAX) return 0;
    if (a->rapid_speed < AXIS_RAPID_SPEED_MIN || a->rapid_speed > AXIS_RAPID_SPEED_MAX) return 0;
    if (a->feed_accel < AXIS_FEED_ACCEL_MIN || a->feed_accel > AXIS_FEED_ACCEL_MAX) return 0;
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
    if (cfg->spindle_ppr < SPINDLE_PPR_MIN || cfg->spindle_ppr > SPINDLE_PPR_MAX) return 0;
    if (cfg->jog_decel_steps > JOG_DECEL_STEPS_MAX) return 0;
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

void config_machine_factory_reset(void) {
    machine_cfg_apply_defaults();
    machine_cfg_write_eeprom();
    DBG_INFO("CFG", "MACH", "factory");
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

uint16_t config_get_jog_decel_steps(void) {
    return machine_cfg.jog_decel_steps;
}

uint32_t config_mm_min_to_sps(uint8_t axis, float mm_min) {  /* мм/мин → steps/s */
    if (mm_min < 1.0f) mm_min = 1.0f;
    float spm = config_get_steps_per_mm(axis);
    uint32_t sps = (uint32_t)((mm_min * spm) / 60.0f);
    if (sps < 1U) sps = 1U;
    return sps;
}

void config_set_motor_steps(uint8_t axis, uint16_t steps) {
    if (steps < AXIS_MOTOR_STEPS_MIN || steps > AXIS_MOTOR_STEPS_MAX) return;
    axis_cfg(axis)->motor_steps = steps;
}

void config_set_microstep(uint8_t axis, uint8_t ms) {
    if (ms < AXIS_MICROSTEP_MIN || ms > AXIS_MICROSTEP_MAX) return;
    axis_cfg(axis)->microstep = ms;
}

void config_set_screw_pitch(uint8_t axis, uint16_t pitch_cents) {
    if (pitch_cents < AXIS_SCREW_PITCH_MIN || pitch_cents > AXIS_SCREW_PITCH_MAX) return;
    axis_cfg(axis)->screw_pitch = pitch_cents;
}

void config_set_max_speed_mm_min(uint8_t axis, uint16_t mm_min) {
    if (mm_min < AXIS_MAX_SPEED_MIN || mm_min > AXIS_MAX_SPEED_MAX) return;
    axis_cfg(axis)->max_speed = mm_min;
}

void config_set_rapid_speed_mm_min(uint8_t axis, uint16_t mm_min) {
    if (mm_min < AXIS_RAPID_SPEED_MIN || mm_min > AXIS_RAPID_SPEED_MAX) return;
    axis_cfg(axis)->rapid_speed = mm_min;
}

void config_set_feed_accel(uint8_t axis, uint8_t accel) {
    if (accel < AXIS_FEED_ACCEL_MIN || accel > AXIS_FEED_ACCEL_MAX) return;
    axis_cfg(axis)->feed_accel = accel;
}

void config_set_dir_invert(uint8_t axis, uint8_t invert) {
    axis_cfg(axis)->dir_invert = invert ? 1U : 0U;
}

void config_set_spindle_ppr(uint16_t ppr) {
    if (ppr < SPINDLE_PPR_MIN || ppr > SPINDLE_PPR_MAX) return;
    machine_cfg.spindle_ppr = ppr;
}

void config_set_jog_decel_steps(uint16_t steps) {
    if (steps > JOG_DECEL_STEPS_MAX) return;
    machine_cfg.jog_decel_steps = steps;
}
