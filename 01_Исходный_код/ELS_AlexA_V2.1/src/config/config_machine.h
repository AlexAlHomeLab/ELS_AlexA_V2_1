#ifndef CONFIG_MACHINE_H
#define CONFIG_MACHINE_H

#include "config_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

void config_machine_load(void);
void config_machine_save(void);

float config_get_steps_per_mm(uint8_t axis);
uint16_t config_get_motor_steps(uint8_t axis);
uint8_t config_get_microstep(uint8_t axis);
uint16_t config_get_screw_pitch(uint8_t axis);
uint16_t config_get_max_speed_mm_min(uint8_t axis);
uint16_t config_get_rapid_speed_mm_min(uint8_t axis);
uint8_t config_get_feed_accel(uint8_t axis);
uint8_t config_get_dir_invert(uint8_t axis);
uint16_t config_get_spindle_ppr(void);
uint32_t config_mm_min_to_sps(uint8_t axis, float mm_min);

void config_set_motor_steps(uint8_t axis, uint16_t steps);
void config_set_microstep(uint8_t axis, uint8_t ms);
void config_set_screw_pitch(uint8_t axis, uint16_t pitch_cents);
void config_set_max_speed_mm_min(uint8_t axis, uint16_t mm_min);
void config_set_rapid_speed_mm_min(uint8_t axis, uint16_t mm_min);
void config_set_feed_accel(uint8_t axis, uint8_t accel);
void config_set_dir_invert(uint8_t axis, uint8_t invert);
void config_set_spindle_ppr(uint16_t ppr);

#ifdef __cplusplus
}
#endif

#endif
