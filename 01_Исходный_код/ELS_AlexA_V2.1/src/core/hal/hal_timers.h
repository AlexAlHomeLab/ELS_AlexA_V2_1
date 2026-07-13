#ifndef HAL_TIMERS_H
#define HAL_TIMERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* --- Инициализация таймеров --- */
void timer1_init(uint16_t period_us);
void timer2_init(void);
void timer3_init(uint16_t ms);
void timer4_init(uint16_t ms);

/* --- Внешние функции, вызываемые из прерываний --- */
void stepper_generate_steps(void);
void spindle_encoder_handler(void);
void ui_pot_read(void);


#ifdef __cplusplus
}
#endif
#endif
