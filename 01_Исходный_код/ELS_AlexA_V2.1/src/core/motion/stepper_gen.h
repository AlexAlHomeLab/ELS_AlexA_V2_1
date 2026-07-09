#ifndef STEPPER_GEN_H
#define STEPPER_GEN_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


/* --- Оси --- */



/* --- Структура состояния оси --- */
typedef struct {
uint32_t accumulator;
uint32_t step_increment;
int32_t position;
int32_t target_position;
uint8_t direction;
uint8_t enabled;
uint16_t microsteps;
} AxisState_t;

/* --- Основные функции --- */
void dds_init(void);
void dds_set_speed(uint8_t axis, uint32_t steps_per_sec);
void dds_set_direction(uint8_t axis, uint8_t dir);
void dds_enable(uint8_t axis, uint8_t enable);
void stepper_generate_steps(void);
int32_t dds_get_position(uint8_t axis);
void dds_set_position(uint8_t axis, int32_t pos);
void dds_set_target(uint8_t axis, int32_t target);


#ifdef __cplusplus
}
#endif
#endif
