#ifndef CONFIG_DISPLAY_H
#define CONFIG_DISPLAY_H

/* Единицы отображения координат на LCD (EEPROM $70, меню CrdU). */

#include "config_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

void config_display_load(void);
void config_display_save(void);
void config_display_factory_reset(void);

uint8_t config_get_coord_units(void);  /* COORD_UNIT_* */
void config_set_coord_units(uint8_t units);

char config_coord_unit_flag(void);  /* 'S' шаги, 'M' мм, 'D' дюймы */

#ifdef __cplusplus
}
#endif

#endif
