#ifndef FSM_CYCLES_H
#define FSM_CYCLES_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


/* --- Типы циклов --- */



/* --- Основные функции --- */
void fsm_cycle_start(uint8_t type, uint8_t axis, float depth, uint8_t passes);
void fsm_cycle_process(void);
void fsm_cycle_stop(void);
void fsm_cycle_pause(void);
void fsm_cycle_resume(void);
void fsm_cycle_ext_process(void);
void fsm_cycle_int_process(void);


#ifdef __cplusplus
}
#endif
#endif
