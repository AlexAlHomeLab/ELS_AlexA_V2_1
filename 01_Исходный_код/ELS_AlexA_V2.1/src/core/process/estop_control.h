#ifndef ESTOP_CONTROL_H
#define ESTOP_CONTROL_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


void estop_check(void);
void estop_trigger(void);
void estop_reset(void);
uint8_t estop_get_state(void);
uint8_t estop_is_triggered(void);


#ifdef __cplusplus
}
#endif
#endif
