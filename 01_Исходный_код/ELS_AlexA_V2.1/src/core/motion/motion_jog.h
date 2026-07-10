#ifndef MOTION_JOG_H
#define MOTION_JOG_H

/* Ручные подачи: джойстик, РГИ (MPG), переход к программным лимитам.
 * hand_pos — накопленное смещение от РГИ с момента нажатия джойстика. */

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void motion_jog_init(void);       /* сброс hand_pos, MPG, нулевые координаты */
void motion_jog_on_axis_select(uint8_t new_axis);  /* стоп MPG при смене селектора оси */
void motion_jog_poll(void);         /* опрос РГИ (main loop) */
void motion_jog_joy_poll(void);   /* опрос джойстика подачи */
void motion_jog_resume(void);     /* сброс jog/MPG/go_lim после паузы FSM */

int32_t motion_jog_get_pos(uint8_t axis);   /* текущая позиция, шаги */
int32_t motion_jog_get_hand(uint8_t axis);  /* hand_pos РГИ, шаги */
void motion_jog_reset_pos(uint8_t axis);    /* обнулить hand_pos */
void motion_jog_zero_axis(uint8_t axis);    /* ноль координаты + rebase лимитов */

void motion_jog_go_limit(uint8_t idx);        /* idx 0..3: rapid к лимиту */
void motion_jog_go_limit_latch(uint8_t idx);  /* к лимиту, стоп при джойстике */

#ifdef __cplusplus
}
#endif

#endif
