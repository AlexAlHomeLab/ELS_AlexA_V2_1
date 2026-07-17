#include "../../config/config.h"
#include "../hal/hal_pins.h"
#include "ui_lcd.h"

#include <Arduino.h>
#include <string.h>

#if !defined(LCD_DISABLE_TX)
#if defined(USE_LCD_SAFE_ASYNC)
#include <LiquidCrystalSafeAsync.h>
/* 6 пинов — без RW; maxRetries задаётся в ui_lcd_init (7-й аргумент неоднозначен с RW-конструктором). */
static LiquidCrystalSafeAsync lcd(LCD_RS_PIN, LCD_EN_PIN,
    LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
#elif defined(USE_LCD_STANDARD)
#include <LiquidCrystal.h>
#if defined(LCD_BUS_8BIT)
/* 10 пинов: RS, EN, D0..D7 — без RW (RW на плате к GND). */
LiquidCrystal lcd(
    LCD_RS_PIN, LCD_RW_PIN, LCD_EN_PIN,
    LCD_D0_PIN, LCD_D1_PIN, LCD_D2_PIN, LCD_D3_PIN,
    LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
#else
static LiquidCrystal lcd(LCD_RS_PIN, LCD_EN_PIN,
    LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
#endif
#endif
#endif /* !LCD_DISABLE_TX */

static char lcd_buffer[LCD_ROWS][LCD_COLS + 1];
static uint8_t lcd_cursor_line = 0xFF;
static uint8_t lcd_cursor_col = 0;

#if !defined(LCD_DISABLE_TX) && defined(USE_LCD_SAFE_ASYNC) && (LCD_SYNC_INTERVAL_MS > 0)
static unsigned long lcd_last_sync_ms = 0;
#endif

#if !defined(LCD_DISABLE_TX) && defined(USE_LCD_SAFE_ASYNC)
static void lcd_drain_queue(unsigned long us_budget) {
    unsigned long t0 = micros();
    for (;;) {
        long r = lcd.processQueue();
        if (r < 0) break;
        if ((micros() - t0) >= us_budget) break;
    }
}
#endif

/* SafeAsync: drain очереди на HD44780 по бюджету за проход loop (не из ISR). */
void ui_lcd_process_queue(void) {
#if !defined(LCD_DISABLE_TX) && defined(USE_LCD_SAFE_ASYNC)
    lcd_drain_queue(LCD_PROCESS_US_BUDGET);
#endif
}

/* SafeAsync: полный drain — только setup/init (до входа в loop). */
void ui_lcd_flush(void) {
#if !defined(LCD_DISABLE_TX) && defined(USE_LCD_SAFE_ASYNC)
    lcd.flush();
#endif
}

void ui_lcd_init(void) {
#if !defined(LCD_DISABLE_TX)
#if defined(USE_LCD_SAFE_ASYNC)
    lcd.setMaxRetries(LCD_SAFE_MAX_RETRIES);
#endif
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.clear();
#if defined(USE_LCD_SAFE_ASYNC)
    lcd_drain_queue(LCD_FLUSH_US_BUDGET);
#endif
    lcd.noCursor();
#endif
    lcd_cursor_line = 0xFF;
    for (uint8_t i = 0; i < LCD_ROWS; i++) {
        ui_lcd_clear_line(i);
    }
}

/* STANDARD: синхронный вывод. SafeAsync: только enqueue — drain в ui_lcd_process_queue(). */
void ui_lcd_update(void) {
#if defined(LCD_DISABLE_TX)
    return;
#else
    if (lcd_cursor_line >= LCD_ROWS) {
        lcd.noCursor();
    }

    for (uint8_t row = 0; row < LCD_ROWS; row++) {
        lcd.setCursor(0, row);
        for (uint8_t col = 0; col < LCD_COLS; col++) {
            lcd.write((uint8_t)lcd_buffer[row][col]);
        }
    }
    if (lcd_cursor_line < LCD_ROWS) {
        lcd.setCursor(lcd_cursor_col, lcd_cursor_line);
        lcd.cursor();
    }

#if defined(USE_LCD_SAFE_ASYNC) && (LCD_SYNC_INTERVAL_MS > 0)
    unsigned long now = millis();
    if (now - lcd_last_sync_ms >= LCD_SYNC_INTERVAL_MS) {
        lcd.syncAll();
        lcd_last_sync_ms = now;
    }
#endif
#endif /* LCD_DISABLE_TX */
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
    uint8_t i;
    for (i = 0; i < LCD_COLS; i++) {
        char c = text[i];
        lcd_buffer[line][i] = (c != '\0') ? c : ' ';
    }
    lcd_buffer[line][LCD_COLS] = '\0';
}

void ui_lcd_clear_line(uint8_t line) {
    if (line >= LCD_ROWS) return;
    memset(lcd_buffer[line], ' ', LCD_COLS);
    lcd_buffer[line][LCD_COLS] = '\0';
}

void ui_lcd_clear(void) {
#if !defined(LCD_DISABLE_TX)
    lcd.clear();
#if defined(USE_LCD_SAFE_ASYNC)
    lcd_drain_queue(LCD_FLUSH_US_BUDGET);
#endif
    lcd.noCursor();
#endif
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
