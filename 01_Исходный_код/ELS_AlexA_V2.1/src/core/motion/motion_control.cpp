#include "motion_control.h"
#include "stepper_gen.h"
#include "backlash.h"
#include "planner.h"
#include "../../config/config.h"

void motion_init(void) {
    dds_init();
    backlash_init();
}

void motion_move_abs(float x, float z, float speed) {
    planner_add_move(x, z, speed);
}

void motion_move_rel(float dx, float dz, float speed) {
    float cx = (float)dds_get_position(AXIS_X);
    float cz = (float)dds_get_position(AXIS_Z);
    planner_add_move(cx + dx, cz + dz, speed);
}

void motion_stop(void) {
    planner_stop_all();
    dds_set_target(AXIS_X, dds_get_position(AXIS_X));
    dds_set_target(AXIS_Z, dds_get_position(AXIS_Z));
    dds_enable(AXIS_X, 0);
    dds_enable(AXIS_Z, 0);
    dds_set_speed(AXIS_X, 0);
    dds_set_speed(AXIS_Z, 0);
}

uint8_t motion_is_moving(void) {
    return !planner_is_empty();
}

float motion_get_pos_x(void) {
    return (float)dds_get_position(AXIS_X) / STEPS_PER_MM_X;
}

float motion_get_pos_z(void) {
    return (float)dds_get_position(AXIS_Z) / STEPS_PER_MM_Z;
}

void motion_set_pos_x(float x) {
    dds_set_position(AXIS_X, (int32_t)(x * STEPS_PER_MM_X));
}

void motion_set_pos_z(float z) {
    dds_set_position(AXIS_Z, (int32_t)(z * STEPS_PER_MM_Z));
}
