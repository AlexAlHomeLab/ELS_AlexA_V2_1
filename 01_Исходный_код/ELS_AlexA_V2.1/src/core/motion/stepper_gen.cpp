#include "stepper_gen.h"
#include "../hal/hal_pins.h"
#include "../../config/config.h"
#include <string.h>

static AxisState_t axis_x;
static AxisState_t axis_z;

void dds_init(void) {
    memset(&axis_x, 0, sizeof(AxisState_t));
    memset(&axis_z, 0, sizeof(AxisState_t));
}

void dds_set_speed(uint8_t axis, uint32_t steps_per_sec) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->step_increment = steps_per_sec;
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

void stepper_generate_steps(void) {
    if (axis_x.enabled) {
        axis_x.accumulator += axis_x.step_increment;
        if (axis_x.accumulator >= 0x80000000UL) {
            axis_x.accumulator -= 0x80000000UL;
            STEP_X_ON();
            axis_x.position += (axis_x.direction ? 1 : -1);
            STEP_X_OFF();
        }
    }
    if (axis_z.enabled) {
        axis_z.accumulator += axis_z.step_increment;
        if (axis_z.accumulator >= 0x80000000UL) {
            axis_z.accumulator -= 0x80000000UL;
            STEP_Z_ON();
            axis_z.position += (axis_z.direction ? 1 : -1);
            STEP_Z_OFF();
        }
    }
}

int32_t dds_get_position(uint8_t axis) {
    return (axis == AXIS_X) ? axis_x.position : axis_z.position;
}

void dds_set_position(uint8_t axis, int32_t pos) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->position = pos;
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
