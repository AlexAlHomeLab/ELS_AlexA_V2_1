#ifndef CONFIG_DISPLAY_H
#define CONFIG_DISPLAY_H

/* Единицы CrdU и режим X (радиус/диаметр) на LCD (EEPROM $70+). */

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

uint8_t config_get_x_coord_mode(void);  /* X_COORD_MODE_* */
void config_set_x_coord_mode(uint8_t mode);
char config_x_coord_axis_char(void);  /* 'R' радиус, 'D' диаметр */

#ifdef __cplusplus
}
#endif

#endif
