#ifndef MODE_DIVIDER_H
#define MODE_DIVIDER_H

#include "../../els_types.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

void mode_divider_enter(void);
void mode_divider_exit(void);
void mode_divider_process(void);

void mode_divider_zero_reset(void);
void mode_divider_parts_up(void);
void mode_divider_parts_down(void);
void mode_divider_part_prev(void);
void mode_divider_part_next(void);

uint16_t mode_divider_get_a(void);
uint16_t mode_divider_get_b(void);
float mode_divider_get_angle_deg(void);
float mode_divider_get_r_deg(void);

void mode_divider_format_line1(char *buf, size_t n);
void mode_divider_format_line2(char *buf, size_t n);
void mode_divider_format_angle_center(char *buf, size_t n);

uint8_t mode_divider_is_active(void);
uint8_t mode_divider_lcd_dirty_poll(void);

#ifdef __cplusplus
}
#endif
#endif
