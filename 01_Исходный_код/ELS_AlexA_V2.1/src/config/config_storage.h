#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

/* Общий загрузчик EEPROM (addr 0..3) и глобальные UI-параметры.
 * config_load() вызывает feed, machine, backlash, display. */

#include "../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_EEPROM_MAGIC   0xA5
#define CONFIG_FEED_MAX_DEFAULT 100   /* % ограничения подачи (legacy) */
#define CONFIG_FEED_MAX_MIN     10
#define CONFIG_FEED_MAX_LIMIT   100
#define CONFIG_BUZZER_DEFAULT   1     /* зуммер вкл по умолчанию */

void config_load(void);   /* все подсистемы config_*_load */
void config_save(void);   /* только блок 0..3 (feed_max, buzzer) */

uint8_t config_get_feed_max(void);    /* % */
uint8_t config_get_buzzer_on(void);   /* 0/1 */
void config_set_feed_max(uint8_t pct);
void config_set_buzzer_on(uint8_t on);

#ifdef __cplusplus
}
#endif

#endif
