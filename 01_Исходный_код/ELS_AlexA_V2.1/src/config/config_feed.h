#ifndef CONFIG_FEED_H
#define CONFIG_FEED_H

#include "../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_FEED_RANGE_COUNT 2
#define CONFIG_FEED_MODE_COUNT 8
#define CONFIG_FEED_MODE_ASYNC 0
#define CONFIG_FEED_MODE_SYNC  1
#define CONFIG_FEED_MODE_THREAD 2

typedef enum {
    FEED_RANGE_ASYNC = 0,
    FEED_RANGE_SYNC = 1,
} FeedRangeId_t;

typedef enum {
    FEED_UNIT_MM_MIN = 0,
    FEED_UNIT_MM_REV = 1,
    FEED_UNIT_NONE = 2,
} FeedUnit_t;

void config_feed_load(void);
void config_feed_save(void);
uint8_t config_feed_uses_pot(uint8_t mode);
FeedUnit_t config_feed_get_unit(uint8_t mode);
uint16_t config_feed_get_min_raw(FeedRangeId_t range);
uint16_t config_feed_get_max_raw(FeedRangeId_t range);
void config_feed_set_range(FeedRangeId_t range, uint16_t min_raw, uint16_t max_raw);
float config_feed_get_min(uint8_t mode);
float config_feed_get_max(uint8_t mode);
float config_feed_map_adc(uint8_t mode, uint16_t adc);

#ifdef __cplusplus
}
#endif

#endif
