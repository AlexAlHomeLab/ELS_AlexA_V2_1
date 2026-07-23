#ifndef UI_LCD_H
#define UI_LCD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_lcd_init(void);
void ui_lcd_update(void);          /* STANDARD: sync; SafeAsync: enqueue из lcd_buffer */
void ui_lcd_process_queue(void);   /* SafeAsync: drain по LCD_PROCESS_US_BUDGET; STANDARD: no-op */
void ui_lcd_flush(void);           /* SafeAsync: полный drain (setup); STANDARD: no-op */
/* Запрос полной перерисовки HD44780 (смена режима и т.п.); выполняет loop. */
void ui_lcd_request_hard_redraw(void);
/* 1 — был запрос; сбрасывает флаг (вызывать из loop). */
uint8_t ui_lcd_take_hard_redraw(void);
/* Аппаратный clear + вывод lcd_buffer + flush; буфер не стирает. */
void ui_lcd_hard_redraw(void);
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
