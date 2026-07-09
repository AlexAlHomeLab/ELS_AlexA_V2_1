#ifndef PLANNER_H
#define PLANNER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* --- Типы --- */
typedef struct {
float target_x;
float target_z;
float speed;
float entry_speed;
float exit_speed;
float distance;
uint8_t direction_x;
uint8_t direction_z;
uint8_t active;
} PlannerBlock_t;

/* --- Основные функции --- */
int planner_add_move(float x, float z, float speed);
void planner_calculate_junctions(void);
PlannerBlock_t* planner_get_next(void);
void planner_stop_all(void);
void planner_process(void);
uint8_t planner_is_empty(void);
uint8_t planner_is_busy(void);

/* --- Параметры --- */
void planner_set_acceleration(float accel);
void planner_set_max_speed(float speed);


#ifdef __cplusplus
}
#endif
#endif
