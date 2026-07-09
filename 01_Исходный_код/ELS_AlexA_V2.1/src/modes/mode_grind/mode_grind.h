#ifndef MODE_GRIND_H
#define MODE_GRIND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void mode_grind_enter(void);
void mode_grind_exit(void);
void mode_grind_process(void);
void mode_grind_start(float depth, uint8_t passes);
void mode_grind_stop(void);
uint8_t mode_grind_is_active(void);
void mode_grind_set_depth(float depth);
void mode_grind_set_passes(uint8_t passes);
float mode_grind_get_depth(void);
uint8_t mode_grind_get_passes(void);
void mode_grind_cycle_process(void);

#ifdef __cplusplus
}
#endif

#endif