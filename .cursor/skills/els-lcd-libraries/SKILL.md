---
name: els-lcd-libraries
description: >-
  Выбор и интеграция драйверов HD44780 (LiquidCrystal, LiquidCrystalSafeAsync) для ELS AlexA V2.1
  на Arduino Mega 2560. Используй при добавлении/смене LCD-библиотеки, условной компиляции USE_LCD_*,
  ui_lcd backend, processQueue, flush/syncAll; когда упоминают LiquidCrystalSafeAsync, Async LCD,
  Timer5 для дисплея или артефакты на LCD под нагрузкой stepper/MPG.
---

# Библиотеки дисплея HD44780 (ELS AlexA V2.1)

## Принцип

Весь доступ к железу LCD — **только через** `ui_lcd.cpp`. Остальной код (`ui_menu`, `.ino`, режимы) вызывает `ui_lcd_set_line*` / `ui_lcd_update` / `ui_lcd_process_queue` — **не** `lcd.print` напрямую.

Содержимое экрана (меню, координаты, Feed) — skill `els-display-menu`. Этот skill — **драйверный слой** и выбор библиотеки.

## Timer5 — не использовать для LCD

**Решение проекта:** обновление дисплея **только из `loop()`**, не из ISR.

| Было (устарело) | Сейчас |
|-----------------|--------|
| `ISR(TIMER5_COMPA_vect) → ui_lcd_update()` | **Запрещено** для любой LCD-библиотеки |
| `timer5_init()` для периодического flush | **Не вызывать**; Timer5 для LCD не резервировать |

**Почему:**
- стандартная `LiquidCrystal` блокирует и может отключать прерывания;
- `LiquidCrystalSafeAsync` требует `processQueue()` из main loop — из ISR не работает;
- вложенность Timer5 (LCD) + Timer1 (stepper) давала риск переполнения стека и джиттера шагов.

**Поток flush (актуальный):**

```
loop @ LCD_UPDATE_MS (80 ms), если lcd_dirty:
  update_lcd() → ui_lcd_set_line* → lcd_buffer[][]
  ui_lcd_update()              → вывод на HD44780
  [SafeAsync] ui_lcd_process_queue() / flush()
```

Каждый проход `loop()` при `USE_LCD_SAFE_ASYNC` — короткий `ui_lcd_process_queue()` (квант мкс).

При правках `hal_timers.cpp`: **удалить** `timer5_init`, `ISR(TIMER5_COMPA_vect)` и include `ui_lcd.h`, если трогаешь файл. Не восстанавливать Timer5 для LCD.

## Выбор библиотеки (условная компиляция)

Конфиг: `src/config/config_lcd.h` (подключается из `config.h`).

| Дефайн | Библиотека | Когда |
|--------|------------|-------|
| `USE_LCD_STANDARD` | `<LiquidCrystal.h>` (Arduino AVR) | по умолчанию, совместимость, минимум RAM |
| `USE_LCD_SAFE_ASYNC` | `LiquidCrystalSafeAsync` | EMI, верификация записи, неблокирующий вывод |

Ровно **один** дефайн активен; оба сразу — `#error`.

```cpp
// config_lcd.h — Arduino IDE: раскомментировать один вариант
// #define USE_LCD_SAFE_ASYNC
// #define USE_LCD_STANDARD

#if !defined(USE_LCD_SAFE_ASYNC) && !defined(USE_LCD_STANDARD)
#define USE_LCD_STANDARD
#endif
```

Установка SafeAsync: `01_Исходный_код/ELS_AlexA_V2.1/libraries/LiquidCrystalSafeAsync/` (или внешний путь в `libraries/` IDE).

## Аппаратура (общая для обоих вариантов)

Из `hal_pins.h` — **4-bit без RW** (типичная разводка Олега А. / Wokwi):

| Сигнал | Пин |
|--------|-----|
| RS | 8 |
| EN | 9 |
| D4–D7 | 10–13 |

```cpp
// USE_LCD_STANDARD и USE_LCD_SAFE_ASYNC (без RW):
lcd(RS, EN, D4, D5, D6, D7 [, maxRetries])
```

Опционально `LCD_RW_PIN` + конструктор с RW — только если пин разведён на плате (верификация по busy-flag).

## Backend в ui_lcd.cpp

| Функция | STANDARD | SAFE_ASYNC |
|---------|----------|------------|
| `ui_lcd_init` | `begin`, `clear` | + `flush()` после init |
| `ui_lcd_update` | синхронный `print` 4 строк | `print` → очередь |
| `ui_lcd_process_queue` | пустая заглушка | `processQueue()` в бюджете `LCD_PROCESS_US_BUDGET` |
| `ui_lcd_clear`, курсор | как есть | API совместим |

Параметры SafeAsync в `config_lcd.h`:

```cpp
#define LCD_SAFE_MAX_RETRIES    3
#define LCD_PROCESS_US_BUDGET   100   /* мкс за вызов в loop */
#define LCD_SYNC_INTERVAL_MS    0     /* 0=выкл; >0 период syncAll() */
```

После полного обновления экрана — `lcd.flush()` или расширенный квант `process_queue` (~2000 мкс), как в примере библиотеки.

## Память (ATmega2560)

| | STANDARD | SAFE_ASYNC (доп.) |
|--|----------|-------------------|
| RAM | `lcd_buffer` ~84 B | +очередь 128 B + внутр. буфер 80 B ≈ +210 B |
| ISR | не трогать LCD | **никогда** LCD из ISR |

Перед релизом SafeAsync — проверить RAM в `.elf`.

## Быстрый старт для агента

1. Прочитай [reference.md](reference.md) — карта файлов, API SafeAsync, чеклист миграции.
2. Правь **только** `config_lcd.h`, `ui_lcd.cpp` / `ui_lcd.h`, `loop()` (вызов `process_queue`), при необходимости `hal_timers.cpp` (удаление Timer5).
3. Не трогай `ui_menu`, форматирование в `.ino` — они не зависят от библиотеки.
4. Связанный skill: `els-display-menu` — что выводить; этот skill — **как** выводить.
5. Перед нетривиальными правками: кратко **что** и **зачем** (`safe-minimal-changes`).

## Чеклист при смене/добавлении библиотеки

```
- [ ] Один USE_LCD_* в config_lcd.h
- [ ] Весь lcd.* только в ui_lcd.cpp
- [ ] Нет ui_lcd_update / processQueue в ISR
- [ ] Timer5 не используется для LCD (удалён или пустой)
- [ ] loop: process_queue каждый проход (SafeAsync)
- [ ] loop: flush после ui_lcd_update при смене экрана (SafeAsync)
- [ ] Сборка STANDARD — регрессия без изменений поведения
- [ ] Сборка SAFE_ASYNC — меню, главный экран, курсор edit
- [ ] Jog + MPG + refresh 80 ms — без артефактов на LCD
```

## Типичные ошибки

1. **`lcd.print` вне `ui_lcd.cpp`** — ломает буфер и переключение библиотек.
2. **Timer5 / любой ISR для flush** — джиттер stepper, несовместимо с SafeAsync.
3. **SafeAsync без `process_queue` в loop** — экран пустой или отстаёт.
4. **Оба USE_LCD_* сразу** — конфликт линковки/логики.
5. **`syncAll()` каждый кадр** — ~2 ms на полный экран; только по таймеру при битых символах.
6. **Путать с I2C** — проект только параллельный HD44780.

## Связанные навыки

- `els-display-menu` — раскладка строк, меню, CrdU
- `els-rgi-mpg` — строка MPG (источник данных, не драйвер)
- `els-limits` — маркеры `< > F B` в координатах

## Дополнительно

- Детали API и миграция: [reference.md](reference.md)
- Пример сравнения библиотек: `LiquidCrystalSafeAsync/examples/test/`
- Аппаратура: `02_Документация/HARDWARE.md`, `hal_pins.h`
