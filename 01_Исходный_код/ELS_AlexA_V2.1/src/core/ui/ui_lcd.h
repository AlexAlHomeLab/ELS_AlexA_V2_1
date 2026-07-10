#ifndef UI_LCD_H
#define UI_LCD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_lcd_init(void);
void ui_lcd_update(void);
void ui_lcd_set_line(uint8_t line, const char *text);
void ui_lcd_set_line_raw(uint8_t line, const char *text);
void ui_lcd_clear_line(uint8_t line);
void ui_lcd_clear(void);
void ui_lcd_set_cursor(uint8_t line, uint8_t col);
void ui_lcd_clear_cursor(void);

#ifdef __cplusplus
}
#endif

#endif
