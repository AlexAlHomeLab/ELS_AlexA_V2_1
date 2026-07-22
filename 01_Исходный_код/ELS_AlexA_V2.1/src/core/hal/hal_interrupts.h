#ifndef HAL_INTERRUPTS_H
#define HAL_INTERRUPTS_H

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* INT0 — шпиндель (Rising на B/D21, ×1 к PPR); INT2 — РГИ */
void encoder_interrupts_init(void);
void mpg_int_enable(void);
void mpg_int_disable(void);

#ifdef __cplusplus
}
#endif

#endif
