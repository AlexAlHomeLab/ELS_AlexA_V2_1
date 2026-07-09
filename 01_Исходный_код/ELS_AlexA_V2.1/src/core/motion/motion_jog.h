#ifndef MOTION_JOG_H
#define MOTION_JOG_H

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void motion_jog_init(void);
void motion_jog_poll(void);
void motion_jog_joy_poll(void);
int32_t motion_jog_get_pos(uint8_t axis);
int32_t motion_jog_get_hand(uint8_t axis);
void motion_jog_reset_pos(uint8_t axis);

#ifdef __cplusplus
}
#endif

#endif
