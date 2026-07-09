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

/* --- Получение позиции --- */
float motion_get_pos_x(void);
float motion_get_pos_z(void);
void motion_set_pos_x(float x);
void motion_set_pos_z(float z);


#ifdef __cplusplus
}
#endif
#endif
