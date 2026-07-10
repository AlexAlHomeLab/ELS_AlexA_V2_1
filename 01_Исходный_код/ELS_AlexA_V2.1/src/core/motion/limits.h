#ifndef LIMITS_H
#define LIMITS_H

/* Программные лимиты (4 кнопки: LimL/F/R/Re) и проверка аппаратных концевиков.
 * LimL/LimR — ось Z, LimF/LimRe — ось X. */

#include "../../config/config_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- UI: кнопки лимитов и LED --- */
void limits_ui_init(void);
void limits_ui_on_click(uint8_t idx);   /* idx 0..3: установить/снять лимит */
void limits_ui_on_hold(uint8_t idx);    /* удержание: ноль оси */

/* Цель для motion_jog_go_limit*; 1 если лимит idx активен */
uint8_t limits_ui_go_target(uint8_t idx, uint8_t *axis, int32_t *target);

/* Ближайший лимит по направлению sign (+1/-1) для оси */
uint8_t limits_ui_go_target_dir(uint8_t axis, int8_t sign, uint8_t *idx, int32_t *target);

uint8_t limits_ui_led_on(uint8_t idx);
char limits_lcd_marker(uint8_t axis);  /* маркер на LCD: '<' '>' 'F' 'B' или ' ' */

/* Сдвиг координат лимитов при обнулении оси (old_pos вычитается) */
void limits_rebase_axis(uint8_t axis, int32_t old_pos);

/* --- Проверка и ограничение движения --- */
uint8_t limits_check(int32_t x_pos, int32_t z_pos);  /* 1 если в пределах */
int32_t limits_clamp_steps(uint8_t axis, int32_t target);

void limits_set(uint8_t axis, int32_t value, uint8_t is_max);  /* программная установка */
uint8_t limits_hardware_check(void);  /* 1 если аппаратные концевики не сработали */

void limits_set_axis(uint8_t axis, uint8_t is_min);  /* зарезервировано */
int32_t limits_get_min(uint8_t axis);
int32_t limits_get_max(uint8_t axis);
uint8_t limits_is_active(uint8_t axis);  /* есть хотя бы один лимит на оси */

#ifdef __cplusplus
}
#endif

#endif
