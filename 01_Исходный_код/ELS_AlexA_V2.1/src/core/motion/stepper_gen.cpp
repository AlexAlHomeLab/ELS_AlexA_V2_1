#include "stepper_gen.h"
#include "../hal/hal_pins.h"
#include "backlash.h"
#include "../../config/config.h"
#include <string.h>

static AxisState_t axis_x;
static AxisState_t axis_z;

uint32_t dds_calc_increment(uint32_t steps_per_sec) {
    return (uint32_t)(((uint64_t)steps_per_sec << 31) / STEP_ISR_FREQ_HZ);
}

void dds_init(void) {
    memset(&axis_x, 0, sizeof(AxisState_t));
    memset(&axis_z, 0, sizeof(AxisState_t));
}

static void dir_x_set(uint8_t d) {
    if (config_get_dir_invert(AXIS_X)) d = !d;
    DIR_X_SET(d);
}
static void dir_z_set(uint8_t d) {
    if (config_get_dir_invert(AXIS_Z)) d = !d;
    DIR_Z_SET(d);
}

void dds_set_speed(uint8_t axis, uint32_t steps_per_sec) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->step_increment = dds_calc_increment(steps_per_sec);
}

void dds_set_direction(uint8_t axis, uint8_t dir) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->direction = dir;
    if (axis == AXIS_X) {
        dir_x_set(dir);
    } else {
        dir_z_set(dir);
    }
}

void dds_enable(uint8_t axis, uint8_t enable) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->enabled = enable;
}

static void step_x_on(void) { STEP_X_ON(); }
static void step_x_off(void) { STEP_X_OFF(); }
static void step_z_on(void) { STEP_Z_ON(); }
static void step_z_off(void) { STEP_Z_OFF(); }

static void dds_axis_step(uint8_t axis, AxisState_t *a, void (*step_on)(void), void (*step_off)(void),
                          void (*dir_set)(uint8_t)) {
    if (!a->enabled || a->step_increment == 0) return;
    if (a->position == a->target_position && backlash_pending(axis) <= 0) return;

    uint8_t fwd = a->direction;
    if (a->position != a->target_position) {
        fwd = (a->target_position > a->position) ? 1U : 0U;
        if (a->direction != fwd) {
            a->direction = fwd;
            dir_set(fwd);
            backlash_arm_axis(axis, fwd, 1);
        }
    } else {
        dir_set(fwd);
    }

    a->accumulator += a->step_increment;
    if (a->accumulator >= 0x80000000UL) {
        a->accumulator -= 0x80000000UL;
        step_on();
        if (!backlash_consume_step(axis, fwd)) {
            a->position += fwd ? 1 : -1;
        }
        step_off();
    }
}

void stepper_generate_steps(void) {
    dds_axis_step(AXIS_X, &axis_x, step_x_on, step_x_off, dir_x_set);
    dds_axis_step(AXIS_Z, &axis_z, step_z_on, step_z_off, dir_z_set);
}

int32_t dds_get_position(uint8_t axis) {
    return (axis == AXIS_X) ? axis_x.position : axis_z.position;
}

void dds_set_position(uint8_t axis, int32_t pos) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->position = pos;
    a->target_position = pos;
}

void dds_set_target(uint8_t axis, int32_t target) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->target_position = target;
}

int32_t dds_get_target(uint8_t axis) {
    return (axis == AXIS_X) ? axis_x.target_position : axis_z.target_position;
}

uint8_t dds_at_target(uint8_t axis) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    if (backlash_pending(axis) > 0) return 0;
    return a->position == a->target_position;
}

uint8_t dds_get_direction(uint8_t axis) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    return a->direction;
}

void dds_reset_accumulator(void) {
    axis_x.accumulator = 0;
    axis_z.accumulator = 0;
}
