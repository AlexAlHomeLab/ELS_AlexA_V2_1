#ifndef MODE_SYNC_H
#define MODE_SYNC_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


void mode_sync_enter(void);
void mode_sync_exit(void);
void mode_sync_process(void);
void mode_sync_start(float feed);
void mode_sync_stop(void);
uint8_t mode_sync_is_active(void);


#ifdef __cplusplus
}
#endif
#endif
