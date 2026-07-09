#ifndef LIMITS_H
#define LIMITS_H






#include "../../config/config_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Программные лимиты --- */
void limits_ui_init(void);
void limits_ui_on_click(uint8_t idx);
void limits_ui_on_hold(uint8_t idx);
uint8_t limits_ui_go_target(uint8_t idx, uint8_t *axis, int32_t *target);
uint8_t limits_ui_led_on(uint8_t idx);
void limits_rebase_axis(uint8_t axis, int32_t old_pos);

uint8_t limits_check(int32_t x_pos, int32_t z_pos);
int32_t limits_clamp_steps(uint8_t axis, int32_t target);
void limits_set(uint8_t axis, int32_t value, uint8_t is_max);
uint8_t limits_hardware_check(void);
void limits_set_axis(uint8_t axis, uint8_t is_min);
int32_t limits_get_min(uint8_t axis);
int32_t limits_get_max(uint8_t axis);
uint8_t limits_is_active(uint8_t axis);




#ifdef __cplusplus
}
#endif
#endif
