#include "backlash.h"
#include "stepper_gen.h"
#include "../../config/config.h"
#include "../../config/config_machine.h"
#include "../debug/debug_serial.h"
#include <Arduino.h>

static int32_t backlash_steps_x = 0;
static int32_t backlash_steps_z = 0;
static uint8_t last_dir_x = BACKLASH_REF_DIR_X;
static uint8_t last_dir_z = BACKLASH_REF_DIR_Z;
static uint8_t synced_x = 0;
static uint8_t synced_z = 0;
static volatile int32_t rem_x = 0;
static volatile int32_t rem_z = 0;

#define BL_STARTUP_TIMEOUT_MS 60000UL

static uint8_t bl_startup_active = 0;
static uint8_t bl_startup_axis = 0xFF;
static unsigned long bl_startup_t0 = 0;

static void backlash_arm_one(uint8_t axis, uint8_t new_dir, uint8_t enable)
{
    uint8_t synced = (axis == AXIS_Z) ? synced_z : synced_x;
    uint8_t last_dir = (axis == AXIS_Z) ? last_dir_z : last_dir_x;
    int32_t steps = (axis == AXIS_Z) ? backlash_steps_z : backlash_steps_x;
    volatile int32_t *rem = (axis == AXIS_Z) ? &rem_z : &rem_x;

    if (!enable || !backlash_enabled() || steps <= 0) {
        return;
    }

    if (*rem > 0) {
        return;
    }

    if (!synced) {
        switch (BACKLASH_FIRST_MOVE) {
            case BACKLASH_FIRST_SKIP:
                return;
            case BACKLASH_FIRST_ALWAYS:
                *rem = steps;
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
}

static void backlash_load_steps_from_config(void)
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

static void backlash_queue_takeup(uint8_t axis, uint8_t dir, int32_t steps)
{
    volatile int32_t *rem = (axis == AXIS_X) ? &rem_x : &rem_z;

    if (!backlash_enabled() || steps <= 0) {
        return;
    }
    if (*rem > 0) {
        return;
    }
    *rem = steps;
    DBG_BL("ARM", axis, dir, steps);
}

void backlash_reload_steps(void)
{
    backlash_load_steps_from_config();
    DBG_INFO_VAL("MOT", "BL", "Xstp", (uint32_t)backlash_steps_x);
    DBG_INFO_VAL("MOT", "BL", "Zstp", (uint32_t)backlash_steps_z);
}

void backlash_init(void)
{
    backlash_load_steps_from_config();
    DBG_INFO_VAL("MOT", "BL", "Xstp", (uint32_t)backlash_steps_x);
    DBG_INFO_VAL("MOT", "BL", "Zstp", (uint32_t)backlash_steps_z);

    last_dir_x = BACKLASH_REF_DIR_X;
    last_dir_z = BACKLASH_REF_DIR_Z;
    synced_x = 0;
    synced_z = 0;
    rem_x = 0;
    rem_z = 0;
}

static void backlash_startup_axis_begin(uint8_t axis)
{
    int32_t steps = (axis == AXIS_X) ? backlash_steps_x : backlash_steps_z;
    uint8_t ref = (axis == AXIS_X) ? BACKLASH_REF_DIR_X : BACKLASH_REF_DIR_Z;
    uint32_t sps = config_mm_min_to_sps(axis, (float)config_backlash_get_auto_speed());

    backlash_queue_takeup(axis, ref, steps);
    dds_set_direction(axis, ref);
    dds_set_target(axis, dds_get_position(axis));
    dds_set_speed(axis, sps);
    dds_enable(axis, 1);
    bl_startup_axis = axis;
    DBG_BL("ARM", axis, ref, steps);
}

static void backlash_startup_axis_stop(uint8_t axis)
{
    dds_enable(axis, 0);
    dds_set_speed(axis, 0);
}

static void backlash_startup_axis_done(uint8_t axis)
{
    uint8_t ref = (axis == AXIS_X) ? BACKLASH_REF_DIR_X : BACKLASH_REF_DIR_Z;
    backlash_startup_axis_stop(axis);
    backlash_sync_axis(axis, ref);
}

void backlash_startup_begin(void)
{
    bl_startup_active = 0;
    bl_startup_axis = 0xFF;

    if (!backlash_enabled() || !config_backlash_get_auto_on()) {
        DBG_INFO("MOT", "BL", "auto skip");
        return;
    }

    DBG_INFO("MOT", "BL", "auto start");

    if (backlash_steps_x > 0) {
        backlash_startup_axis_begin(AXIS_X);
        bl_startup_active = 1;
        bl_startup_t0 = millis();
        return;
    }
    if (backlash_steps_z > 0) {
        backlash_startup_axis_begin(AXIS_Z);
        bl_startup_active = 1;
        bl_startup_t0 = millis();
        return;
    }

    DBG_INFO("MOT", "BL", "auto zero");
}

void backlash_startup_poll(void)
{
    if (!bl_startup_active || bl_startup_axis > AXIS_Z) {
        return;
    }

    if (millis() - bl_startup_t0 >= BL_STARTUP_TIMEOUT_MS) {
        backlash_startup_axis_stop(bl_startup_axis);
        backlash_abort_pending();
        bl_startup_active = 0;
        bl_startup_axis = 0xFF;
        DBG_INFO("MOT", "BL", "auto timeout");
        return;
    }

    if (backlash_pending(bl_startup_axis) > 0) {
        return;
    }

    backlash_startup_axis_done(bl_startup_axis);

    if (bl_startup_axis == AXIS_X && backlash_steps_z > 0) {
        backlash_startup_axis_begin(AXIS_Z);
        bl_startup_t0 = millis();
        return;
    }

    bl_startup_active = 0;
    bl_startup_axis = 0xFF;
    DBG_INFO("MOT", "BL", "auto done");
}

uint8_t backlash_startup_busy(void)
{
    return bl_startup_active;
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
    DBG_BL("SYNC", axis, dir, 0);
}

void backlash_arm_axis(uint8_t axis, uint8_t new_dir, uint8_t enable)
{
    if (axis == AXIS_X) {
        backlash_arm_one(AXIS_X, new_dir, enable);
    } else {
        backlash_arm_one(AXIS_Z, new_dir, enable);
    }
}

void backlash_abort_pending(void)
{
    if (rem_x > 0 || rem_z > 0) {
        DBG_INFO_VAL("MOT", "BL", "ABORT", (uint32_t)(rem_x + rem_z));
    }
    rem_x = 0;
    rem_z = 0;
}

int32_t backlash_pending(uint8_t axis)
{
    return (axis == AXIS_X) ? rem_x : rem_z;
}

uint8_t backlash_consume_step(uint8_t axis, uint8_t dir)
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
