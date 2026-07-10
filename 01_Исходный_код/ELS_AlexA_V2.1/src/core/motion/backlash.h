#ifndef BACKLASH_H
#define BACKLASH_H

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Выборка люфта: шаги в ISR без изменения логической координаты (grblHAL). */

void backlash_init(void);
void backlash_reload_steps(void);
void backlash_startup_begin(void);
void backlash_startup_poll(void);
uint8_t backlash_startup_busy(void);
void backlash_sync_axis(uint8_t axis, uint8_t dir);
void backlash_arm_axis(uint8_t axis, uint8_t new_dir, uint8_t enable);
void backlash_abort_pending(void);

int32_t backlash_pending(uint8_t axis);
uint8_t backlash_consume_step(uint8_t axis, uint8_t dir);

uint8_t backlash_enabled(void);
void backlash_set_steps(uint8_t axis, int32_t steps);
int32_t backlash_get_steps(uint8_t axis);

#ifdef __cplusplus
}
#endif

#endif
