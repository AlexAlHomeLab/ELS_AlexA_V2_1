#ifndef MOTION_JOG_H
#define MOTION_JOG_H

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void motion_jog_init(void);
void motion_jog_poll(void);
void motion_jog_joy_poll(void);
void motion_jog_resume(void);
int32_t motion_jog_get_pos(uint8_t axis);
int32_t motion_jog_get_hand(uint8_t axis);
void motion_jog_reset_pos(uint8_t axis);
void motion_jog_zero_axis(uint8_t axis);
void motion_jog_go_limit(uint8_t idx);
void motion_jog_go_limit_latch(uint8_t idx);

#ifdef __cplusplus
}
#endif

#endif
