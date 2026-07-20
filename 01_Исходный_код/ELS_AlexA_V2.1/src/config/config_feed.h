#ifndef CONFIG_FEED_H
#define CONFIG_FEED_H

/* Диапазоны потенциометра подачи: async (мм/мин) и sync (мм/об).
 * EEPROM addr 16..25. */

#include "../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_FEED_RANGE_COUNT 2        /* число диапазонов: async + sync */
#define CONFIG_FEED_MODE_COUNT 8         /* число режимов FSM (1..8) */
#define CONFIG_FEED_MODE_ASYNC  0        /* индекс: асинхронная подача */
#define CONFIG_FEED_MODE_SYNC   1        /* индекс: синхронная подача */
#define CONFIG_FEED_MODE_THREAD 2        /* индекс: резьба (без пота) */

/* --- Диапазоны потенциометра (заводские, EEPROM) --- */
#define CONFIG_FEED_ASYNC_MIN_DEFAULT   20     /* заводской мин. async, мм/мин */
#define CONFIG_FEED_ASYNC_MAX_DEFAULT   1200   /* заводской макс. async, мм/мин */
#define CONFIG_FEED_SYNC_MIN_DEFAULT    1      /* заводской мин. sync ×100: 0.01 мм/об */
#define CONFIG_FEED_SYNC_MAX_DEFAULT    60     /* заводской макс. sync ×100: 0.60 мм/об */
#define CONFIG_FEED_SYNC_RAW_SCALE      0.01f  /* мм/об на единицу raw (×100 → физ.) */
#define CONFIG_FEED_ASYNC_QUANT_STEP    10     /* шаг квантования async, мм/мин */
#define CONFIG_FEED_ASYNC_QUANT_SPAN    50     /* мин. размах диапазона для квантования */

/* --- Допустимые диапазоны в меню настройки пота --- */
#define CONFIG_FEED_ASYNC_EDIT_MIN_LOW  10     /* меню: нижний предел min async */
#define CONFIG_FEED_ASYNC_EDIT_MIN_HIGH 990    /* меню: верхний предел min async */
#define CONFIG_FEED_ASYNC_EDIT_MAX_LOW  20     /* меню: нижний предел max async */
#define CONFIG_FEED_ASYNC_EDIT_MAX_HIGH 3000   /* меню: верхний предел max async */
#define CONFIG_FEED_SYNC_EDIT_MIN_LOW   1      /* меню: нижний предел min sync (raw) */
#define CONFIG_FEED_SYNC_EDIT_MIN_HIGH  199    /* меню: верхний предел min sync (raw) */
#define CONFIG_FEED_SYNC_EDIT_MAX_LOW   2      /* меню: нижний предел max sync (raw) */
#define CONFIG_FEED_SYNC_EDIT_MAX_HIGH  600    /* меню: верхний предел max sync (raw) */

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
