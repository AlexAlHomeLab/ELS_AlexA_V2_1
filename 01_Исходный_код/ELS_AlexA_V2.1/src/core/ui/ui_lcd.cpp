#include "../../config/config.h"
#include "../hal/hal_pins.h"
#include "ui_lcd.h"

#include <LiquidCrystal.h>
#include <string.h>

static LiquidCrystal lcd(LCD_RS_PIN, LCD_EN_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
static char lcd_buffer[LCD_ROWS][LCD_COLS + 1];

void ui_lcd_init(void) {
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.clear();
    for (uint8_t i = 0; i < LCD_ROWS; i++) {
        ui_lcd_clear_line(i);
    }
}

void ui_lcd_update(void) {
    for (uint8_t row = 0; row < LCD_ROWS; row++) {
        lcd.setCursor(0, row);
        lcd.print(lcd_buffer[row]);
    }
}

void ui_lcd_set_line(uint8_t line, const char *text) {
    if (line >= LCD_ROWS || text == NULL) return;
    strncpy(lcd_buffer[line], text, LCD_COLS);
    lcd_buffer[line][LCD_COLS] = '\0';
}

void ui_lcd_clear_line(uint8_t line) {
    if (line >= LCD_ROWS) return;
    memset(lcd_buffer[line], ' ', LCD_COLS);
    lcd_buffer[line][LCD_COLS] = '\0';
}

void ui_lcd_clear(void) {
    lcd.clear();
    for (uint8_t i = 0; i < LCD_ROWS; i++) {
        ui_lcd_clear_line(i);
    }
}
