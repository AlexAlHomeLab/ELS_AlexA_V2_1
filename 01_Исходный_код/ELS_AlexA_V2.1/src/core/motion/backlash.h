#ifndef BACKLASH_H
#define BACKLASH_H

/* Компенсация люфта: выборка при смене направления (rem_x/rem_z).
 * ENABLE_BACKLASH=0 в config_defs.h отключает модуль. */

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void backlash_init(void);
void backlash_reload_steps(void);  /* перечитать шаги из EEPROM/config */

/* Поставить выборку в очередь (dir не используется) */
void backlash_queue_takeup(uint8_t axis, uint8_t dir, int32_t steps);

void backlash_sync_axis(uint8_t axis, uint8_t dir);    /* люфт взяты, last_dir=dir */
void backlash_unsync_axis(uint8_t axis);               /* сброс synced, rem=0 */
void backlash_arm_axis(uint8_t axis, uint8_t new_dir, uint8_t enable);  /* при смене DIR */
void backlash_abort_pending(void);                     /* отмена rem_x/rem_z */

int32_t backlash_pending(uint8_t axis);  /* оставшиеся шаги выборки */

/* 1 если шаг «съеден» люфтом (позиция не меняется) */
uint8_t backlash_consume_step(uint8_t axis, uint8_t dir);

uint8_t backlash_enabled(void);
void backlash_set_steps(uint8_t axis, int32_t steps);
int32_t backlash_get_steps(uint8_t axis);

void backlash_log_poll(void);  /* main: вывод лога START из ISR-очереди */

#ifdef __cplusplus
}
#endif

#endif
