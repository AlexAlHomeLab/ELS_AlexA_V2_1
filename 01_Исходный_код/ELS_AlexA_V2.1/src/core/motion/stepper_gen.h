#ifndef STEPPER_GEN_H
#define STEPPER_GEN_H

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOTION_FLAG_BACKLASH  0x01U
#define MOTION_FLAG_JOG       0x02U
#define MOTION_FLAG_JOG_CRUISE 0x04U

typedef struct {
    int32_t target_x;
    int32_t target_z;
    float nominal_mm_min;
    float entry_mm_min;
    float exit_mm_min;
    uint8_t flags;
    uint8_t bl_axis;
    uint16_t bl_steps;
} MotionCommand_t;

typedef struct {
    uint32_t accumulator;
    uint32_t step_increment;
    int32_t position;
    int32_t target_position;
    uint8_t direction;
    uint8_t enabled;
} AxisState_t;

void dds_init(void);
uint32_t dds_calc_increment(uint32_t steps_per_sec);
void dds_set_speed(uint8_t axis, uint32_t steps_per_sec);
void dds_set_direction(uint8_t axis, uint8_t dir);
void dds_enable(uint8_t axis, uint8_t enable);
void stepper_generate_steps(void);
int32_t dds_get_position(uint8_t axis);
void dds_set_position(uint8_t axis, int32_t pos);
void dds_set_target(uint8_t axis, int32_t target);
int32_t dds_get_target(uint8_t axis);
uint8_t dds_at_target(uint8_t axis);
uint8_t dds_get_direction(uint8_t axis);
void dds_reset_accumulator(void);

void dds_motion_start(const MotionCommand_t *cmd);
uint8_t dds_motion_jog_retarget(const MotionCommand_t *cmd);
void dds_motion_jog_release(void);
void dds_motion_update_target(uint8_t axis, int32_t target);
float dds_motion_get_speed_mm_min(uint8_t axis);
uint8_t dds_motion_busy(void);
uint8_t dds_motion_jog_cruise_active(void);
void dds_motion_stop(void);
void dds_motion_log_poll(void);

#ifdef __cplusplus
}
#endif

#endif
