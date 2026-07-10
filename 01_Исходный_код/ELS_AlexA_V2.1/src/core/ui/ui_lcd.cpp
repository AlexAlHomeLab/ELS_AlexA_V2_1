#include "../../config/config.h"
#include "../hal/hal_pins.h"
#include "ui_lcd.h"

#include <LiquidCrystal.h>
#include <string.h>

static LiquidCrystal lcd(LCD_RS_PIN, LCD_EN_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
static char lcd_buffer[LCD_ROWS][LCD_COLS + 1];
static uint8_t lcd_cursor_line = 0xFF;
static uint8_t lcd_cursor_col = 0;

void ui_lcd_init(void) {
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.clear();
    lcd.noCursor();
    lcd_cursor_line = 0xFF;
    for (uint8_t i = 0; i < LCD_ROWS; i++) {
        ui_lcd_clear_line(i);
    }
}

void ui_lcd_update(void) {
    lcd.noCursor();
    for (uint8_t row = 0; row < LCD_ROWS; row++) {
        lcd.setCursor(0, row);
        lcd.print(lcd_buffer[row]);
    }
    if (lcd_cursor_line < LCD_ROWS) {
        lcd.setCursor(lcd_cursor_col, lcd_cursor_line);
        lcd.cursor();
    }
}

void ui_lcd_set_line(uint8_t line, const char *text) {
    if (line >= LCD_ROWS || text == NULL) return;
    uint8_t n = (uint8_t)strlen(text);
    if (n > LCD_COLS) n = LCD_COLS;
    memcpy(lcd_buffer[line], text, n);
    memset(lcd_buffer[line] + n, ' ', LCD_COLS - n);
    lcd_buffer[line][LCD_COLS] = '\0';
}

void ui_lcd_set_line_raw(uint8_t line, const char *text) {
    if (line >= LCD_ROWS || text == NULL) return;
    memcpy(lcd_buffer[line], text, LCD_COLS);
    lcd_buffer[line][LCD_COLS] = '\0';
}

void ui_lcd_clear_line(uint8_t line) {
    if (line >= LCD_ROWS) return;
    memset(lcd_buffer[line], ' ', LCD_COLS);
    lcd_buffer[line][LCD_COLS] = '\0';
}

void ui_lcd_clear(void) {
    lcd.clear();
    lcd.noCursor();
    lcd_cursor_line = 0xFF;
    for (uint8_t i = 0; i < LCD_ROWS; i++) {
        ui_lcd_clear_line(i);
    }
}

void ui_lcd_set_cursor(uint8_t line, uint8_t col) {
    if (line >= LCD_ROWS || col >= LCD_COLS) return;
    lcd_cursor_line = line;
    lcd_cursor_col = col;
}

void ui_lcd_clear_cursor(void) {
    lcd_cursor_line = 0xFF;
}
