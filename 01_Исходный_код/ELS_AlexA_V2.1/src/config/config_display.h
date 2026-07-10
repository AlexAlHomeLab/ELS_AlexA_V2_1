#ifndef CONFIG_DISPLAY_H
#define CONFIG_DISPLAY_H

#include "config_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

void config_display_load(void);
void config_display_save(void);

uint8_t config_get_coord_units(void);
void config_set_coord_units(uint8_t units);
char config_coord_unit_flag(void);

#ifdef __cplusplus
}
#endif

#endif
