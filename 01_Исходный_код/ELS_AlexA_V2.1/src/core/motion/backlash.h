#ifndef BACKLASH_H
#define BACKLASH_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


/* --- Основные функции --- */
void backlash_init(void);
int32_t backlash_compensate(uint8_t axis, int32_t steps, uint8_t direction);
void backlash_set_distance(uint8_t axis, int32_t distance);
uint8_t backlash_enabled(void);
void backlash_reset(void);
int32_t backlash_get_comp_x(void);
int32_t backlash_get_comp_z(void);

#ifdef __cplusplus
}
#endif
#endif
