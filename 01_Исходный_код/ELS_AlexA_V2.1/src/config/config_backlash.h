#ifndef CONFIG_BACKLASH_H
#define CONFIG_BACKLASH_H

#include "config_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

void config_backlash_load(void);
void config_backlash_save(void);

uint8_t config_backlash_get_auto_on(void);
uint16_t config_backlash_get_steps_x(void);
uint16_t config_backlash_get_steps_z(void);
uint16_t config_backlash_get_auto_speed(void);
uint16_t config_backlash_get_min_speed(void);

void config_backlash_set_auto_on(uint8_t on);
void config_backlash_set_steps_x(uint16_t steps);
void config_backlash_set_steps_z(uint16_t steps);
void config_backlash_set_auto_speed(uint16_t mm_min);
void config_backlash_set_min_speed(uint16_t mm_min);
float config_backlash_comp_speed_mm_min(uint8_t axis, float feed_mm_min);

#ifdef __cplusplus
}
#endif

#endif
