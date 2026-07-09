#ifndef MODE_ASYNC_H
#define MODE_ASYNC_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


/* --- Основные функции --- */
void mode_async_enter(void);
void mode_async_exit(void);
void mode_async_process(void);
void mode_async_handle_manual(void);
void mode_async_cycle_ext_start(void);
void mode_async_cycle_int_start(void);
void mode_async_cycle_process(void);

/* --- Параметры --- */
void mode_async_set_feed(float speed);
void mode_async_set_depth(float depth);
void mode_async_set_passes(uint8_t passes);

/* --- Состояние --- */
uint8_t mode_async_is_active(void);


#ifdef __cplusplus
}
#endif
#endif
