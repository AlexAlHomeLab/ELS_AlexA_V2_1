#ifndef CONFIG_BACKLASH_H
#define CONFIG_BACKLASH_H

/* Настройки компенсации люфта (EEPROM $56..$64, меню BlAu/BlX/BlZ).
 * auto_on — автовыборка при старте; speeds — ограничение скорости выборки. */

#include "config_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

void config_backlash_load(void);
void config_backlash_save(void);
void config_backlash_factory_reset(void);

uint8_t config_backlash_get_auto_on(void);      /* 1 — очередь при старте */
uint16_t config_backlash_get_steps_x(void);     /* 0 — из CENTIMM */
uint16_t config_backlash_get_steps_z(void);
uint16_t config_backlash_get_auto_speed(void);  /* мм/мин, макс. выборки */
uint16_t config_backlash_get_min_speed(void);   /* мм/мин, мин. выборки */

void config_backlash_set_auto_on(uint8_t on);
void config_backlash_set_steps_x(uint16_t steps);
void config_backlash_set_steps_z(uint16_t steps);
void config_backlash_set_auto_speed(uint16_t mm_min);
void config_backlash_set_min_speed(uint16_t mm_min);

/* Ограничить feed в диапазоне [min_speed, auto_speed] и max_speed оси (BlAu) */
float config_backlash_comp_speed_mm_min(uint8_t axis, float feed_mm_min);

/* Скорость выборки при jog/MPG: max(feed, BlMn), потолок max_speed оси */
float config_backlash_runtime_speed_mm_min(uint8_t axis, float feed_mm_min);

#ifdef __cplusplus
}
#endif

#endif
