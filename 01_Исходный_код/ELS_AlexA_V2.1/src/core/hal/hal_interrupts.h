#ifndef HAL_INTERRUPTS_H
#define HAL_INTERRUPTS_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


/* --- Инициализация прерываний --- */
void buttons_interrupts_init(void);
void encoder_interrupts_init(void);
void mpg_interrupts_init(void);

/* --- Обработчики (вызываются из ISR) --- */
void spindle_encoder_process(uint8_t a, uint8_t b);
void mpg_process(uint8_t a, uint8_t b);
void buttons_scan(void);


#ifdef __cplusplus
}
#endif
#endif
