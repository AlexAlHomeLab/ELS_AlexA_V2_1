#include "motion_control.h"
#include "stepper_gen.h"
#include "backlash.h"
#include "planner.h"
#include "../../config/config.h"

static int32_t motion_cur_steps(uint8_t axis) {
    if (dds_at_target(axis)) {
        return dds_get_position(axis);
    }
    return dds_get_target(axis);
}

void motion_init(void) {
    dds_init();
    backlash_init();
}

void motion_move_abs(float x, float z, float speed) {
    planner_add_move(x, z, speed);
}

void motion_move_rel(float dx, float dz, float speed) {
    float cx = motion_get_pos_mm(AXIS_X);
    float cz = motion_get_pos_mm(AXIS_Z);
    planner_add_move(cx + dx, cz + dz, speed);
}

void motion_stop(void) {
    planner_stop_all();
    backlash_abort_pending();
    dds_set_target(AXIS_X, dds_get_position(AXIS_X));
    dds_set_target(AXIS_Z, dds_get_position(AXIS_Z));
}

uint8_t motion_is_moving(void) {
    return planner_is_busy();
}

int32_t motion_get_pos_steps(uint8_t axis) {
    if (axis > AXIS_Z) return 0;
    return motion_cur_steps(axis);
}

float motion_get_pos_mm(uint8_t axis) {
    float spm = (axis == AXIS_X) ? STEPS_PER_MM_X : STEPS_PER_MM_Z;
    return (float)motion_get_pos_steps(axis) / spm;
}

void motion_set_pos_steps(uint8_t axis, int32_t steps) {
    if (axis > AXIS_Z) return;
    dds_set_position(axis, steps);
    dds_set_target(axis, steps);
}

void motion_zero_all(void) {
    motion_set_pos_steps(AXIS_X, 0);
    motion_set_pos_steps(AXIS_Z, 0);
#if ENABLE_BACKLASH
    if (!config_backlash_get_auto_on()) {
        backlash_sync_axis(AXIS_X, BACKLASH_REF_DIR_X);
        backlash_sync_axis(AXIS_Z, BACKLASH_REF_DIR_Z);
    }
#endif
}

float motion_get_pos_x(void) {
    return motion_get_pos_mm(AXIS_X);
}

float motion_get_pos_z(void) {
    return motion_get_pos_mm(AXIS_Z);
}

void motion_set_pos_x(float x) {
    motion_set_pos_steps(AXIS_X, (int32_t)(x * STEPS_PER_MM_X));
}

void motion_set_pos_z(float z) {
    motion_set_pos_steps(AXIS_Z, (int32_t)(z * STEPS_PER_MM_Z));
}
