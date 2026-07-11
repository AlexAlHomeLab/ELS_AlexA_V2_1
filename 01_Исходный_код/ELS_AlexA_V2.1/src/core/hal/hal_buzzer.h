#ifndef HAL_BUZZER_H
#define HAL_BUZZER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void hal_buzzer_init(void);
void hal_buzzer_beep_ms(uint16_t ms);
void hal_buzzer_beep_double_ms(uint16_t ms, uint16_t gap_ms);
void hal_buzzer_estop_signal(void);

#ifdef __cplusplus
}
#endif

#endif
