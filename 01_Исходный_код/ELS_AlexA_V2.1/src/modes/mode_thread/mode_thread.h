#ifndef MODE_THREAD_H
#define MODE_THREAD_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


void mode_thread_enter(void);
void mode_thread_exit(void);
void mode_thread_process(void);
void mode_thread_select_pitch(uint8_t index);
void mode_thread_start(float length, float start_pos, float end_pos);
void mode_thread_stop(void);
uint8_t mode_thread_is_active(void);


#ifdef __cplusplus
}
#endif
#endif
