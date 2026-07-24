/*
 * Тест LCD 2004 в 8-битном режиме (HD44780).
 * Arduino Mega 2560 — пины как в ELS AlexA V2.1 (hal_pins.h).
 * Выводит A..Z циклически, заполняя весь экран 20x4.
 * Выводит время очистки и обновления экрана в Serial.
 */

#include <LiquidCrystal.h>

#define LCD_RS_PIN  8
#define LCD_RW_PIN  5
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

LiquidCrystal lcd(
   LCD_RS_PIN, LCD_RW_PIN, LCD_EN_PIN,
   LCD_D0_PIN, LCD_D1_PIN, LCD_D2_PIN, LCD_D3_PIN,
   LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
    
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
    Serial.begin(115200);  // Инициализация последовательного порта
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.clear();
    fill_alphabet();
    
    Serial.println("=== Тест LCD 2004 8-бит ===");
    Serial.println("Время очистки и обновления экрана:");
    Serial.println("-------------------------------");
}

void loop()
{
    unsigned long start_time, end_time;
    unsigned long clear_time, update_time;
    
    // Измерение времени очистки
    start_time = micros();
    lcd.clear();
    end_time = micros();
    clear_time = end_time - start_time;
    
    // Измерение времени обновления
    start_time = micros();
    fill_alphabet();
    end_time = micros();
    update_time = end_time - start_time;
    
    // Вывод результатов в Serial
    Serial.print("Очистка: ");
    Serial.print(clear_time);
    Serial.print(" мкс (");
    Serial.print(clear_time / 1000.0, 3);
    Serial.println(" мс)");
    
    Serial.print("Обновление: ");
    Serial.print(update_time);
    Serial.print(" мкс (");
    Serial.print(update_time / 1000.0, 3);
    Serial.println(" мс)");
    
    Serial.print("Общее время: ");
    Serial.print(clear_time + update_time);
    Serial.print(" мкс (");
    Serial.print((clear_time + update_time) / 1000.0, 3);
    Serial.println(" мс)");
    Serial.println("-------------------------------");
    
    delay(1000);
}