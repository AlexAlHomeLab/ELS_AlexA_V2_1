#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include "../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_EEPROM_MAGIC 0xA5
#define CONFIG_FEED_MAX_DEFAULT 100
#define CONFIG_FEED_MAX_MIN     10
#define CONFIG_FEED_MAX_LIMIT   100
#define CONFIG_BUZZER_DEFAULT   1

void config_load(void);
void config_save(void);
uint8_t config_get_feed_max(void);
uint8_t config_get_buzzer_on(void);
void config_set_feed_max(uint8_t pct);
void config_set_buzzer_on(uint8_t on);

#ifdef __cplusplus
}
#endif

#endif
