#ifndef SPINDLE_CONTROL_H
#define SPINDLE_CONTROL_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


/* --- Основные функции --- */
void spindle_init(void);
void spindle_poll(void);
void spindle_set_speed(uint16_t pwm);
void spindle_set_direction(uint8_t dir);
void spindle_start(void);
void spindle_stop(void);
uint32_t spindle_get_rpm(void);
uint32_t spindle_get_frequency(void);
void spindle_encoder_isr_tick(void);
void spindle_encoder_isr_step(void);
void spindle_encoder_handler(void);
void spindle_encoder_process(uint8_t a, uint8_t b);

/* --- Направления --- */



/* --- Состояние --- */
typedef struct {
uint16_t pwm;
uint8_t direction;
uint8_t running;
uint32_t rpm;
uint32_t encoder_count;
uint32_t last_encoder_count;
uint8_t phase;
} SpindleState_t;

extern SpindleState_t spindle_state;


#ifdef __cplusplus
}
#endif
#endif
