# Библиотеки LCD — справочник ELS AlexA V2.1

## Файлы

```
01_Исходный_код/ELS_AlexA_V2.1/
├── src/config/config_lcd.h         # USE_LCD_*, бюджеты SafeAsync
├── src/config/config.h             # LCD_COLS/ROWS, LCD_UPDATE_MS, include config_lcd
├── src/core/ui/ui_lcd.cpp          # единственный backend LiquidCrystal / SafeAsync
├── src/core/ui/ui_lcd.h            # ui_lcd_* + ui_lcd_process_queue()
├── src/core/hal/hal_pins.h         # LCD_RS/EN/D4-D7
├── src/core/hal/hal_timers.cpp     # Timer5 для LCD — удалить, не использовать
├── ELS_AlexA_V2.1.ino              # loop: lcd_dirty → update_lcd → ui_lcd_update
└── libraries/
    ├── (встроенная) LiquidCrystal
    └── LiquidCrystalSafeAsync/     # при USE_LCD_SAFE_ASYNC
```

## API ui_lcd (стабильный для проекта)

```c
void ui_lcd_init(void);
void ui_lcd_update(void);                    /* flush lcd_buffer → HD44780 */
void ui_lcd_process_queue(void);           /* SafeAsync: drain очереди; STANDARD: no-op */
void ui_lcd_set_line(uint8_t line, const char *text);
void ui_lcd_set_line_raw(uint8_t line, const char *text);
void ui_lcd_clear_line(uint8_t line);
void ui_lcd_clear(void);
void ui_lcd_set_cursor(uint8_t line, uint8_t col);
void ui_lcd_clear_cursor(void);
```

## config_lcd.h (шаблон)

```cpp
#ifndef CONFIG_LCD_H
#define CONFIG_LCD_H

// Arduino IDE: раскомментировать ОДИН:
// #define USE_LCD_SAFE_ASYNC
// #define USE_LCD_STANDARD

#if !defined(USE_LCD_SAFE_ASYNC) && !defined(USE_LCD_STANDARD)
#define USE_LCD_STANDARD
#endif

#if defined(USE_LCD_SAFE_ASYNC) && defined(USE_LCD_STANDARD)
#error "Только одна библиотека: USE_LCD_SAFE_ASYNC или USE_LCD_STANDARD"
#endif

#if defined(USE_LCD_SAFE_ASYNC)
#define LCD_SAFE_MAX_RETRIES    3
#define LCD_PROCESS_US_BUDGET   100
#define LCD_SYNC_INTERVAL_MS    0
#endif

#endif
```

PlatformIO (если появится): `-DUSE_LCD_SAFE_ASYNC` в `build_flags` env.

## ui_lcd.cpp — условный backend

```cpp
#if defined(USE_LCD_SAFE_ASYNC)
#include <LiquidCrystalSafeAsync.h>
static LiquidCrystalSafeAsync lcd(LCD_RS_PIN, LCD_EN_PIN,
    LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN, LCD_SAFE_MAX_RETRIES);
#elif defined(USE_LCD_STANDARD)
#include <LiquidCrystal.h>
static LiquidCrystal lcd(LCD_RS_PIN, LCD_EN_PIN,
    LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
#endif
```

## LiquidCrystalSafeAsync — отличия от STANDARD

| Аспект | STANDARD | SafeAsync |
|--------|----------|-----------|
| `print` / `setCursor` | блокирует loop | ставит в очередь |
| Физический вывод | сразу | `processQueue()` / `flush()` |
| Верификация DDRAM | нет | да (с RW — по busy; без RW — задержки) |
| Прерывания | может cli надолго | не отключает надолго |
| Диагностика | — | `getErrorCount()`, `setMaxRetries()` |
| Периодическая починка | — | `syncAll()` (~2 ms на 80 ячеек) |

### processQueue в loop

```cpp
void ui_lcd_process_queue(void) {
#if defined(USE_LCD_SAFE_ASYNC)
    unsigned long t0 = micros();
    while ((micros() - t0) < LCD_PROCESS_US_BUDGET) {
        if (lcd.processQueue() < 0) break;
    }
#endif
}
```

### После полного refresh экрана

```cpp
ui_lcd_update();
#if defined(USE_LCD_SAFE_ASYNC)
lcd.flush();   /* или process_queue с budget ~2000 мкс */
#endif
```

## Поток данных (без Timer5)

```
┌─────────────────────────────────────────────┐
│ loop (каждый проход)                        │
│   ui_lcd_process_queue()     [SafeAsync]    │
│   ... poll, motion, FSM ...                 │
│   if (lcd_dirty && 80 ms):                  │
│     update_lcd() → буфер                    │
│     ui_lcd_update() → драйвер               │
│     flush / extended process_queue          │
└─────────────────────────────────────────────┘

ISR Timer1  → stepper_generate_steps()   /* hot path */
ISR Timer2  → spindle_encoder_handler()
ISR Timer3  → wdt_reset()
ISR Timer4  → ui_pot_read()
ISR Timer5  → НЕ LCD (удалить legacy ui_lcd_update)
```

Триггер обновления: `lcd_mark_dirty_if_changed()` в loop → флаг `lcd_dirty` → `update_lcd()` + `ui_lcd_update()`.

## Timer5 — legacy (удалить при касании hal_timers)

В коде может оставаться:

```cpp
void timer5_init(uint16_t ms) { ... }
ISR(TIMER5_COMPA_vect) { ui_lcd_update(); }
```

`timer5_init()` **нигде не вызывается** — ISR мёртвый, но вводит в заблуждение. При работе с таймерами — **удалить** оба блока и объявление в `hal_timers.h`.

## Сравнение с другими библиотеками (не использовать без отдельного ТЗ)

| Библиотека | Статус в проекте |
|------------|------------------|
| LiquidCrystal (AVR) | `USE_LCD_STANDARD` — default |
| LiquidCrystalSafeAsync | `USE_LCD_SAFE_ASYNC` — опционально |
| LiquidCrystal_I2C | нет — дисплей параллельный |
| AsyncLiquidCrystal | не планируется — нет верификации, cli на чтение |
| hd44780 | не планируется — другой API |

## Миграция STANDARD → SafeAsync

1. Положить библиотеку в `libraries/LiquidCrystalSafeAsync/`.
2. Создать/включить `config_lcd.h`, переключить на `USE_LCD_SAFE_ASYNC`.
3. Реализовать backend в `ui_lcd.cpp` + `ui_lcd_process_queue`.
4. В `loop()`: `ui_lcd_process_queue()` в начале; `flush` после `ui_lcd_update`.
5. Убедиться, что Timer5 не вызывает LCD.
6. Собрать, проверить RAM, прогнать чеклист из SKILL.md.

Откат: `#define USE_LCD_STANDARD`, пересборка — без правок меню/форматирования.

## Отладка SafeAsync

- `lcd.getErrorCount()` — растёт при EMI/ошибках записи; опционально в DEBUG или Serial `?`.
- `lcd.isBusy()` — очередь не пуста.
- При «битых» символах: `LCD_SYNC_INTERVAL_MS` 100–500, не каждый кадр.

## Проверка после правок

1. Обе сборки (STANDARD / SAFE_ASYNC) компилируются.
2. Главный экран + меню edit с курсором.
3. Jog + MPG 30 кГц stepper — LCD без пропусков символов.
4. `grep ui_lcd_update` — только loop/setup, **не** ISR.
5. `grep timer5` — нет привязки к LCD.
