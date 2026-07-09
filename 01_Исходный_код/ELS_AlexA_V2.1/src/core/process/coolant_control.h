#ifndef COOLANT_CONTROL_H
#define COOLANT_CONTROL_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


void coolant_on(void);
void coolant_off(void);
void coolant_toggle(void);
uint8_t coolant_get_state(void);


#ifdef __cplusplus
}
#endif
#endif
