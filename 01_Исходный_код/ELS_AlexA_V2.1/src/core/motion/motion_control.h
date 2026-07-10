#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

/* Высокоуровневый API движения: координаты, абс/отн. перемещения, стоп.
 * Позиция в шагах — источник истины (dds_get_position/target). */

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void motion_init(void);  /* dds_init + backlash_init */

void motion_move_abs(float x, float z, float speed);   /* мм, мм/мин → planner */
void motion_move_rel(float dx, float dz, float speed);
void motion_stop(void);          /* planner_stop + abort люфта + freeze target */
uint8_t motion_is_moving(void);  /* planner_is_busy */

/* --- Координаты (шаги — источник истины) --- */
int32_t motion_get_pos_steps(uint8_t axis);  /* фактическая position (без шагов люфта) */
float motion_get_pos_mm(uint8_t axis);
void motion_set_pos_steps(uint8_t axis, int32_t steps);
void motion_zero_all(void);  /* 0,0 + sync люфта если BlAu выкл */

/* --- мм (совместимость UI/режимов) --- */
float motion_get_pos_x(void);
float motion_get_pos_z(void);
void motion_set_pos_x(float x);
void motion_set_pos_z(float z);

#ifdef __cplusplus
}
#endif

#endif
