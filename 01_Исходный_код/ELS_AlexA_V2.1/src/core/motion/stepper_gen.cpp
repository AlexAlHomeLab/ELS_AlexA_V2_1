#include "stepper_gen.h"
#include "../hal/hal_pins.h"
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

void dds_set_speed(uint8_t axis, uint32_t steps_per_sec) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->step_increment = dds_calc_increment(steps_per_sec);
}

void dds_set_direction(uint8_t axis, uint8_t dir) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->direction = dir;
    if (axis == AXIS_X) {
        DIR_X_SET(dir);
    } else {
        DIR_Z_SET(dir);
    }
}

void dds_enable(uint8_t axis, uint8_t enable) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->enabled = enable;
}

static void dir_x_set(uint8_t d) { DIR_X_SET(d); }
static void dir_z_set(uint8_t d) { DIR_Z_SET(d); }
static void step_x_on(void) { STEP_X_ON(); }
static void step_x_off(void) { STEP_X_OFF(); }
static void step_z_on(void) { STEP_Z_ON(); }
static void step_z_off(void) { STEP_Z_OFF(); }

static void dds_axis_step(AxisState_t *a, void (*step_on)(void), void (*step_off)(void),
                          void (*dir_set)(uint8_t)) {
    if (!a->enabled || a->step_increment == 0) return;
    if (a->position == a->target_position) return;

    uint8_t fwd = a->target_position > a->position;
    if (a->direction != fwd) {
        a->direction = fwd;
        dir_set(fwd);
    }

    a->accumulator += a->step_increment;
    if (a->accumulator >= 0x80000000UL) {
        a->accumulator -= 0x80000000UL;
        step_on();
        a->position += fwd ? 1 : -1;
        step_off();
    }
}

void stepper_generate_steps(void) {
    dds_axis_step(&axis_x, step_x_on, step_x_off, dir_x_set);
    dds_axis_step(&axis_z, step_z_on, step_z_off, dir_z_set);
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
    return a->position == a->target_position;
}

void dds_reset_accumulator(void) {
    axis_x.accumulator = 0;
    axis_z.accumulator = 0;
}
