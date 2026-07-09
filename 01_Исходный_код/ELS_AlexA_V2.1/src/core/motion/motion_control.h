#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


/* --- Основные функции --- */
void motion_init(void);
void motion_move_abs(float x, float z, float speed);
void motion_move_rel(float dx, float dz, float speed);
void motion_stop(void);
uint8_t motion_is_moving(void);

/* --- Координаты (шаги — источник истины) --- */
int32_t motion_get_pos_steps(uint8_t axis);
float motion_get_pos_mm(uint8_t axis);
void motion_set_pos_steps(uint8_t axis, int32_t steps);
void motion_zero_all(void);

/* --- мм (совместимость) --- */
float motion_get_pos_x(void);
float motion_get_pos_z(void);
void motion_set_pos_x(float x);
void motion_set_pos_z(float z);


#ifdef __cplusplus
}
#endif
#endif
