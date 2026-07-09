#ifndef UI_POT_H
#define UI_POT_H

#include "../../els_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_pot_init(void);
void ui_pot_read(void);
uint16_t ui_pot_get_value(void);
uint8_t ui_pot_get_percent(void);
uint8_t ui_pot_has_changed(void);
uint8_t ui_pot_get_mode(void);
uint8_t ui_pot_get_submode(void);
uint8_t ui_pot_poll(void);
float ui_pot_get_feed_value(uint8_t mode);
float ui_pot_get_jog_mm_min(void);
void ui_pot_feed_format(char *buf, size_t len, uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif
