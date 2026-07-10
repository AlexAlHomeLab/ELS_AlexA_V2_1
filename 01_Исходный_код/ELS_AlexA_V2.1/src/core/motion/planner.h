#ifndef PLANNER_H
#define PLANNER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLANNER_FLAG_BACKLASH  0x01U

typedef struct {
    float target_x;
    float target_z;
    int32_t target_steps_x;
    int32_t target_steps_z;
    float speed;
    float entry_speed;
    float exit_speed;
    float distance;
    uint8_t direction_x;
    uint8_t direction_z;
    uint8_t flags;
    uint8_t bl_axis;
    uint16_t bl_steps;
    uint8_t active;
} PlannerBlock_t;

int planner_add_move(float x, float z, float speed);
int planner_add_move_steps(int32_t tx, int32_t tz, float speed);
int planner_add_backlash_takeup(uint8_t axis, float speed_mm_min);
void planner_startup_backlash_queue(void);
uint8_t planner_startup_busy(void);

void planner_exec_axis(uint8_t axis, int32_t target_steps, float speed_mm_min, uint8_t jog);
void planner_exec_jog(int32_t tx, int32_t tz, float speed_mm_min, const char *kind, uint8_t cruise);
void planner_jog_stop(void);
void planner_calculate_junctions(void);
PlannerBlock_t* planner_get_next(void);
void planner_stop_all(void);
void planner_process(void);
uint8_t planner_is_empty(void);
uint8_t planner_is_busy(void);
void planner_get_queue_target(float *x, float *z);
void planner_get_queue_target_steps(int32_t *tx, int32_t *tz);

void planner_set_acceleration(float accel);
void planner_set_max_speed(float speed);

#ifdef __cplusplus
}
#endif

#endif
