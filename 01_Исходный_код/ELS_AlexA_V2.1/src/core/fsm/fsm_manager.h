#ifndef FSM_MANAGER_H
#define FSM_MANAGER_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


void fsm_manager_set_mode(uint8_t mode);
void fsm_manager_set_submode(uint8_t submode);
void fsm_manager_init(void);
void fsm_manager_poll(void);
void fsm_manager_enter_mode(uint8_t mode);
void fsm_manager_exit_mode(uint8_t mode);
void fsm_manager_process(void);
uint8_t fsm_manager_get_mode(void);
uint8_t fsm_manager_get_submode(void);


#ifdef __cplusplus
}
#endif
#endif
