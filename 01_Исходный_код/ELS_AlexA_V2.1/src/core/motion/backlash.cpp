#include "backlash.h"
#include "../../config/config.h"
#include "../../config/config_machine.h"
#include "../../config/config_backlash.h"
#include "../debug/debug_serial.h"
#include <Arduino.h>
#include <avr/interrupt.h>

#define BL_LOG_QSIZE 4U

typedef struct {
    uint8_t axis;
    uint8_t dir;
    int32_t steps;
} BlLogItem_t;

static volatile uint8_t bl_log_w;
static volatile uint8_t bl_log_r;
static BlLogItem_t bl_log_q[BL_LOG_QSIZE];

static void bl_log_push(uint8_t axis, uint8_t dir, int32_t steps)
{
    uint8_t sreg = SREG;
    uint8_t next;

    cli();
    next = (uint8_t)((bl_log_w + 1U) % BL_LOG_QSIZE);
    if (next != bl_log_r) {
        bl_log_q[bl_log_w].axis = axis;
        bl_log_q[bl_log_w].dir = dir;
        bl_log_q[bl_log_w].steps = steps;
        bl_log_w = next;
    }
    SREG = sreg;
}

static void bl_log_start(uint8_t axis, uint8_t dir, int32_t steps)
{
    bl_log_push(axis, dir, steps);
}

void backlash_log_poll(void)
{
    while (bl_log_r != bl_log_w) {
        BlLogItem_t item;
        uint8_t sreg = SREG;

        cli();
        if (bl_log_r == bl_log_w) {
            SREG = sreg;
            break;
        }
        item = bl_log_q[bl_log_r];
        bl_log_r = (uint8_t)((bl_log_r + 1U) % BL_LOG_QSIZE);
        SREG = sreg;

        DBG_BL("START", item.axis, item.dir, item.steps);
    }
}

static int32_t backlash_steps_x = 0;   /* шаги выборки X */
static int32_t backlash_steps_z = 0;   /* шаги выборки Z */
static uint8_t last_dir_x = BACKLASH_REF_DIR_X;  /* последнее физ. направление */
static uint8_t last_dir_z = BACKLASH_REF_DIR_Z;
static uint8_t armed_dir_x;  /* направление текущей выборки (пока rem>0) */
static uint8_t armed_dir_z;
static uint8_t synced_x = 0;   /* 1 — люфт синхронизирован */
static uint8_t synced_z = 0;
static volatile int32_t rem_x = 0;  /* остаток выборки (ISR) */
static volatile int32_t rem_z = 0;
static float bl_feed_x = 0.0f;      /* подача при arm, мм/мин */
static float bl_feed_z = 0.0f;
static uint32_t bl_inc_x = 0U;      /* DDS increment выборки X */
static uint32_t bl_inc_z = 0U;      /* DDS increment выборки Z */
static uint32_t bl_min_sps_x = 1U;  /* BlMn → steps/s, кэш */
static uint32_t bl_min_sps_z = 1U;
static uint32_t bl_max_sps_x = 1U;  /* BlSp → steps/s, кэш */
static uint32_t bl_max_sps_z = 1U;

/* DDS increment без stepper_gen (ISR-safe) */
static uint32_t bl_calc_increment(uint32_t sps)
{
    if (sps < 1U) sps = 1U;
    return (uint32_t)(((uint64_t)sps << 31) / STEP_ISR_FREQ_HZ);
}

static void bl_reload_sps_cache(void)
{
    bl_min_sps_x = config_mm_min_to_sps(AXIS_X, (float)config_backlash_get_min_speed());
    bl_min_sps_z = config_mm_min_to_sps(AXIS_Z, (float)config_backlash_get_min_speed());
    bl_max_sps_x = config_mm_min_to_sps(AXIS_X, (float)config_backlash_get_auto_speed());
    bl_max_sps_z = config_mm_min_to_sps(AXIS_Z, (float)config_backlash_get_auto_speed());
}

/* feed_sps → bl_inc; clamp [BlMn, BlSp] (как до привязки к feed) */
static void bl_store_arm_speed(uint8_t axis, uint32_t feed_sps)
{
    uint32_t min_sps;
    uint32_t max_sps;
    uint32_t sps;
    uint32_t inc;

    if (axis == AXIS_X) {
        min_sps = bl_min_sps_x;
        max_sps = bl_max_sps_x;
    } else {
        min_sps = bl_min_sps_z;
        max_sps = bl_max_sps_z;
    }
    sps = feed_sps;
    if (sps < min_sps) sps = min_sps;
    if (sps > max_sps) sps = max_sps;
    inc = bl_calc_increment(sps);
    if (inc < 1U) inc = 1U;

    if (axis == AXIS_X) {
        bl_inc_x = inc;
    } else {
        bl_inc_z = inc;
    }

    /* bl_feed — только в main (в ISR без float) */
    if (SREG & (uint8_t)(1U << 7)) {
        float spm = config_get_steps_per_mm(axis);
        float feed_mm_min = (spm >= 1.0f) ? ((float)sps * 60.0f / spm)
                                          : (float)config_backlash_get_min_speed();
        if (feed_mm_min < 1.0f) {
            feed_mm_min = (float)config_backlash_get_min_speed();
        }
        if (axis == AXIS_X) {
            bl_feed_x = feed_mm_min;
        } else {
            bl_feed_z = feed_mm_min;
        }
    }
}

/* Поставить rem=steps при смене направления (BACKLASH_FIRST_MOVE) */
static void backlash_arm_one(uint8_t axis, uint8_t new_dir, uint8_t enable, uint32_t feed_sps)
{
    uint8_t synced = (axis == AXIS_Z) ? synced_z : synced_x;
    uint8_t last_dir = (axis == AXIS_Z) ? last_dir_z : last_dir_x;
    uint8_t *armed_dir = (axis == AXIS_Z) ? &armed_dir_z : &armed_dir_x;
    int32_t steps = (axis == AXIS_Z) ? backlash_steps_z : backlash_steps_x;
    volatile int32_t *rem = (axis == AXIS_Z) ? &rem_z : &rem_x;

    if (!enable || !backlash_enabled() || steps <= 0) {
        return;
    }

    if (*rem > 0) {
        if (new_dir == *armed_dir) {
            return;
        }
        *rem = 0;
    }

    if (!synced) {
        switch (BACKLASH_FIRST_MOVE) {
            case BACKLASH_FIRST_SKIP:
                return;
            case BACKLASH_FIRST_ALWAYS:
                *rem = steps;
                *armed_dir = new_dir;
                bl_store_arm_speed(axis, feed_sps);
                bl_log_start(axis, new_dir, steps);
                return;
            case BACKLASH_FIRST_REF:
            default:
                break;
        }
    }

    if (new_dir == last_dir) {
        return;
    }

    *rem = steps;
    *armed_dir = new_dir;
    bl_store_arm_speed(axis, feed_sps);
    bl_log_start(axis, new_dir, steps);
}

static void backlash_load_steps_from_config(void)  /* EEPROM или CENTIMM по умолчанию */
{
#ifdef ENABLE_BACKLASH
    float spm_x = config_get_steps_per_mm(AXIS_X);
    float spm_z = config_get_steps_per_mm(AXIS_Z);
    uint16_t cx = config_backlash_get_steps_x();
    uint16_t cz = config_backlash_get_steps_z();

    if (cx > 0) {
        backlash_steps_x = cx;
    } else {
        backlash_steps_x = (int32_t)(BACKLASH_X_CENTIMM * 0.01f * spm_x + 0.5f);
    }
    if (cz > 0) {
        backlash_steps_z = cz;
    } else {
        backlash_steps_z = (int32_t)(BACKLASH_Z_CENTIMM * 0.01f * spm_z + 0.5f);
    }
#else
    backlash_steps_x = 0;
    backlash_steps_z = 0;
#endif
}

void backlash_queue_takeup(uint8_t axis, uint8_t dir, int32_t steps)
{
    volatile int32_t *rem = (axis == AXIS_X) ? &rem_x : &rem_z;

    if (!backlash_enabled() || steps <= 0) {
        return;
    }
    if (*rem > 0) {
        return;
    }
    if (axis == AXIS_X) {
        armed_dir_x = dir;
    } else {
        armed_dir_z = dir;
    }
    *rem = steps;
    bl_store_arm_speed(axis, (axis == AXIS_X) ? bl_min_sps_x : bl_min_sps_z);
    bl_log_start(axis, dir, steps);
}

void backlash_reload_steps(void)
{
    backlash_load_steps_from_config();
    bl_reload_sps_cache();
    bl_inc_x = bl_calc_increment(bl_min_sps_x);
    bl_inc_z = bl_calc_increment(bl_min_sps_z);
    DBG_INFO_VAL("MOT", "BL", "Xstp", (uint32_t)backlash_steps_x);
    DBG_INFO_VAL("MOT", "BL", "Zstp", (uint32_t)backlash_steps_z);
}

void backlash_init(void)
{
    backlash_load_steps_from_config();
    bl_reload_sps_cache();
    DBG_INFO_VAL("MOT", "BL", "Xstp", (uint32_t)backlash_steps_x);
    DBG_INFO_VAL("MOT", "BL", "Zstp", (uint32_t)backlash_steps_z);

    last_dir_x = BACKLASH_REF_DIR_X;
    last_dir_z = BACKLASH_REF_DIR_Z;
    synced_x = 0;
    synced_z = 0;
    rem_x = 0;
    rem_z = 0;
    armed_dir_x = BACKLASH_REF_DIR_X;
    armed_dir_z = BACKLASH_REF_DIR_Z;
    bl_feed_x = 0.0f;
    bl_feed_z = 0.0f;
    bl_inc_x = bl_calc_increment(bl_min_sps_x);
    bl_inc_z = bl_calc_increment(bl_min_sps_z);
    bl_log_w = 0;
    bl_log_r = 0;
}

void backlash_sync_axis(uint8_t axis, uint8_t dir)
{
    if (axis == AXIS_X) {
        last_dir_x = dir;
        synced_x = 1;
        rem_x = 0;
    } else {
        last_dir_z = dir;
        synced_z = 1;
        rem_z = 0;
    }
}

void backlash_unsync_axis(uint8_t axis)
{
    if (axis == AXIS_X) {
        synced_x = 0;
        rem_x = 0;
    } else {
        synced_z = 0;
        rem_z = 0;
    }
}

void backlash_arm_axis(uint8_t axis, uint8_t new_dir, uint8_t enable, uint32_t feed_sps)
{
    if (axis == AXIS_X) {
        backlash_arm_one(AXIS_X, new_dir, enable, feed_sps);
    } else {
        backlash_arm_one(AXIS_Z, new_dir, enable, feed_sps);
    }
}

uint32_t backlash_get_arm_inc(uint8_t axis)
{
    uint32_t inc = (axis == AXIS_X) ? bl_inc_x : bl_inc_z;

    if (inc < 1U) {
        inc = bl_calc_increment((axis == AXIS_X) ? bl_min_sps_x : bl_min_sps_z);
    }
    return inc;
}

float backlash_get_arm_feed_mm_min(uint8_t axis)
{
    float feed = (axis == AXIS_X) ? bl_feed_x : bl_feed_z;

    if (feed < 1.0f) {
        feed = (float)config_backlash_get_min_speed();
    }
    return feed;
}

void backlash_abort_pending(void)
{
    uint8_t sreg = SREG;
    int32_t rx;
    int32_t rz;

    cli();
    rx = rem_x;
    rz = rem_z;
    rem_x = 0;
    rem_z = 0;
    SREG = sreg;

    if (rx > 0 || rz > 0) {
        DBG_INFO_VAL("MOT", "BL", "ABORT", (uint32_t)(rx + rz));
    }
}

int32_t backlash_pending(uint8_t axis)
{
    return (axis == AXIS_X) ? rem_x : rem_z;
}

uint8_t backlash_consume_step(uint8_t axis, uint8_t dir)  /* 1 = шаг «съеден» люфтом */
{
    volatile int32_t *rem = (axis == AXIS_X) ? &rem_x : &rem_z;
    uint8_t *synced = (axis == AXIS_X) ? &synced_x : &synced_z;
    uint8_t *last_dir = (axis == AXIS_X) ? &last_dir_x : &last_dir_z;

    if (*rem <= 0) {
        return 0;
    }

    (*rem)--;
    if (*rem == 0) {
        *last_dir = dir;
        *synced = 1;
    }
    return 1;
}

uint8_t backlash_enabled(void)
{
#ifdef ENABLE_BACKLASH
    return 1;
#else
    return 0;
#endif
}

void backlash_set_steps(uint8_t axis, int32_t steps)
{
    if (steps < 0) steps = 0;
    if (axis == AXIS_X) {
        backlash_steps_x = steps;
    } else {
        backlash_steps_z = steps;
    }
}

int32_t backlash_get_steps(uint8_t axis)
{
    return (axis == AXIS_X) ? backlash_steps_x : backlash_steps_z;
}
