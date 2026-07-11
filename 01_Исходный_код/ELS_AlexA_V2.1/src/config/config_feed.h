#ifndef CONFIG_FEED_H
#define CONFIG_FEED_H

/* Диапазоны потенциометра подачи: async (мм/мин) и sync (мм/об).
 * EEPROM addr 16..25. */

#include "../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_FEED_RANGE_COUNT 2
#define CONFIG_FEED_MODE_COUNT 8
#define CONFIG_FEED_MODE_ASYNC  0
#define CONFIG_FEED_MODE_SYNC   1
#define CONFIG_FEED_MODE_THREAD 2

/* --- Диапазоны потенциометра (заводские, EEPROM) --- */
#define CONFIG_FEED_ASYNC_MIN_DEFAULT   20    /* мм/мин */
#define CONFIG_FEED_ASYNC_MAX_DEFAULT   400   /* мм/мин */
#define CONFIG_FEED_SYNC_MIN_DEFAULT    1     /* x100: 0.01 мм/об (7e2 MIN_FEED) */
#define CONFIG_FEED_SYNC_MAX_DEFAULT    60    /* x100: 0.60 мм/об (7e2 MAX_FEED) */
#define CONFIG_FEED_SYNC_RAW_SCALE      0.01f /* мм/об на единицу raw */
#define CONFIG_FEED_ASYNC_QUANT_STEP    10    /* шаг квантования мм/мин */
#define CONFIG_FEED_ASYNC_QUANT_SPAN    50    /* мин. размах для квантования */

/* --- Допустимые диапазоны в меню настройки пота --- */
#define CONFIG_FEED_ASYNC_EDIT_MIN_LOW  10
#define CONFIG_FEED_ASYNC_EDIT_MIN_HIGH 990
#define CONFIG_FEED_ASYNC_EDIT_MAX_LOW  20
#define CONFIG_FEED_ASYNC_EDIT_MAX_HIGH 1000
#define CONFIG_FEED_SYNC_EDIT_MIN_LOW   1
#define CONFIG_FEED_SYNC_EDIT_MIN_HIGH  199
#define CONFIG_FEED_SYNC_EDIT_MAX_LOW   2
#define CONFIG_FEED_SYNC_EDIT_MAX_HIGH  200

typedef enum {
    FEED_RANGE_ASYNC = 0,  /* мм/мин */
    FEED_RANGE_SYNC = 1,   /* мм/об (raw × SCALE) */
} FeedRangeId_t;

typedef enum {
    FEED_UNIT_MM_MIN = 0,
    FEED_UNIT_MM_REV = 1,
    FEED_UNIT_NONE = 2,    /* Thread — без пота */
} FeedUnit_t;

void config_feed_load(void);
void config_feed_save(void);
void config_feed_factory_reset(void);

uint8_t config_feed_uses_pot(uint8_t mode);       /* 0 для Thread */
FeedUnit_t config_feed_get_unit(uint8_t mode);
uint16_t config_feed_get_min_raw(FeedRangeId_t range);
uint16_t config_feed_get_max_raw(FeedRangeId_t range);
void config_feed_set_range(FeedRangeId_t range, uint16_t min_raw, uint16_t max_raw);
float config_feed_get_min(uint8_t mode);          /* в физ. единицах */
float config_feed_get_max(uint8_t mode);
float config_feed_map_adc(uint8_t mode, uint16_t adc);  /* ADC → подача */

#ifdef __cplusplus
}
#endif

#endif
