#ifndef FSM_CORE_H
#define FSM_CORE_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


/* --- Состояния --- */
typedef enum {
STATE_IDLE,
STATE_MANUAL,
STATE_CYCLE,
STATE_PAUSED,
STATE_ERROR,
STATE_ANY
} State_t;

/* --- Структура перехода --- */
typedef struct {
State_t from;
State_t to;
uint8_t event;
} Transition_t;

/* --- Коды ошибок --- */






/* --- Основные функции --- */
uint8_t fsm_validate_transition(State_t from, State_t to);
void fsm_transition(State_t new_state);
State_t fsm_get_state(void);
void fsm_error(uint8_t error_code);
void fsm_emergency_stop(void);
void fsm_set_mode(uint8_t mode);
void fsm_set_submode(uint8_t submode);

/* --- Обработчики состояний --- */
void fsm_enter(State_t state);
void fsm_exit(State_t state);
void fsm_process(State_t state);


#ifdef __cplusplus
}
#endif
#endif
