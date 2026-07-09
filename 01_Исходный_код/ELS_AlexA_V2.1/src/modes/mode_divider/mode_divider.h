#ifndef MODE_DIVIDER_H
#define MODE_DIVIDER_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


void mode_divider_enter(void);
void mode_divider_exit(void);
void mode_divider_set_parts(uint16_t n);
void mode_divider_next(void);
void mode_divider_reset(void);
float mode_divider_get_angle(void);
void mode_divider_process(void);


#ifdef __cplusplus
}
#endif
#endif
