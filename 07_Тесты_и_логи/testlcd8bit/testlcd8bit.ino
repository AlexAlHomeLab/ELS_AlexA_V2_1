/*
 * Тест LCD 2004 в 8-битном режиме (HD44780).
 * Arduino Mega 2560 — пины как в ELS AlexA V2.1 (hal_pins.h).
 * Выводит A..Z циклически, заполняя весь экран 20x4.
 */

#include <LiquidCrystal.h>

#define LCD_RS_PIN  8
#define LCD_RW_PIN  6
#define LCD_EN_PIN  9
#define LCD_D0_PIN  50
#define LCD_D1_PIN  51
#define LCD_D2_PIN  52
#define LCD_D3_PIN  53
#define LCD_D4_PIN  10
#define LCD_D5_PIN  11
#define LCD_D6_PIN  12
#define LCD_D7_PIN  13

#define LCD_COLS  20
#define LCD_ROWS  4

/* RS, RW, EN, D0..D7 — 8-бит + RW */
LiquidCrystal lcd(LCD_RS_PIN, LCD_EN_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);

// LiquidCrystal lcd(
//     LCD_RS_PIN, LCD_EN_PIN,
//     LCD_D0_PIN, LCD_D1_PIN, LCD_D2_PIN, LCD_D3_PIN,
//     LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
//LiquidCrystal lcd(
//    LCD_RS_PIN, LCD_RW_PIN, LCD_EN_PIN,
//    LCD_D0_PIN, LCD_D1_PIN, LCD_D2_PIN, LCD_D3_PIN,
//    LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
    
static void fill_alphabet(void)
{
    char ch = 'A';
    for (uint8_t row = 0; row < LCD_ROWS; row++) {
        lcd.setCursor(0, row);
        for (uint8_t col = 0; col < LCD_COLS; col++) {
            lcd.write(ch);
            if (++ch > 'Z') {
                ch = 'A';
            }
        }
    }
}

void setup()
{
    pinMode(6, OUTPUT);
    digitalWrite(6, LOW);
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.clear();
    fill_alphabet();
}

void loop()
{
    lcd.clear();
    fill_alphabet();
    delay(1000);
}
