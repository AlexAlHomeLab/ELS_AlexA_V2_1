---
name: els-display-menu
description: >-
  Спецификация и реализация LCD 2004, главного экрана и меню параметров для ELS AlexA V2.1
  на Arduino Mega 2560. Используй при разработке, отладке и ревью ui_lcd, ui_menu,
  config_display, форматирования координат и строки Feed; когда пользователь упоминает
  дисплей, LCD, меню, CrdU, единицы координат, ui_menu_lcd или главный экран.
---

# Меню и дисплей (ELS AlexA V2.1)

## Спецификация (ТЗ)

##Дисплей LCD 2004
Четыре строки по 20 символов, параллельный интерфейс HD44780 (без I2C).
Главный экран: режим, подача, подрежим, строка РГИ, координаты X/Z с маркерами лимитов.
Единицы координат: шаги (S), мм (M), дюймы (D) — параметр меню CrdU, флаг в последнем столбце.
Меню параметров: длинное удержание Select — вход; короткий клик Select — редактирование;
длинное удержание Select в меню — сохранение в EEPROM и выход.
Кнопки под дисплеем: Left/Right/Up/Down/Select — навигация и редактирование меню.

## Быстрый старт для агента

1. Прочитай [reference.md](reference.md) — карта файлов, API, таблица параметров, раскладка строк.
2. Правь **только** ui_lcd / ui_menu / config_display / форматирование в `.ino`; не трогай motion без запроса.
3. Перед правками: кратко опиши **что** и **зачем**; жди подтверждения при нетривиальной логике.
4. После правок пройди чеклист ниже.
5. Маркеры лимитов — skill `els-limits`; строка MPG — `els-rgi-mpg`; Feed/pot — `els-joy-feed`.

## Карта кода (кратко)

| Слой | Файл | Роль |
|------|------|------|
| HAL | `hal_pins.h` | `LCD_*_PIN`, `BTN_*_PIN` |
| HAL | `hal_timers.cpp` | Timer5 ISR → `ui_lcd_update()` |
| UI | `ui_lcd.cpp` | буфер 4×20, `set_line`, курсор |
| UI | `ui_menu.cpp` | меню 27 параметров, browse/edit, EEPROM save |
| UI | `ui_buttons.cpp` | клики Select/Up/Down/L/R для меню |
| Конфиг | `config_display.cpp` | CrdU, EEPROM $70 |
| Главный экран | `ELS_AlexA_V2.1.ino` | `update_main_lcd()`, формат координат |
| Feed | `ui_pot.cpp` | `ui_pot_feed_format()` — строка F: |

## Главный экран (4 строки)

| Строка | Содержимое | Источник |
|--------|------------|----------|
| 0 | `[Mode][----Feed----][Sub]` | `lcd_format_status_line`: mode слева, Feed по центру, Man/Ext/Int справа |
| 1 | `MPG X +1234........M` | `lcd_format_mpg_line`; последний символ — `config_coord_unit_flag()` |
| 2 | пустая | `ui_lcd_clear_line(2)` |
| 3 | `X 12.345<Z -0.001>` | `lcd_format_coords_line` + `limits_lcd_marker` |

При `planner_startup_busy()` (backlash takeup): строка 0 = `BL takeup...`, строка 1 = `Wait`, строка 3 = координаты.

Переключение экранов в `update_lcd()`: если `ui_menu_is_active()` → `ui_menu_lcd()`, иначе `update_main_lcd()`.

## Меню параметров

### Режимы

| Режим | Управление |
|-------|------------|
| **Browse** | Up/Down — параметр; короткий Select — Edit; длинный Select (600 ms) — save+exit |
| **Edit** | L/R — цифра; Up/Down — ±цифра; длинный Select — confirm |

Вход в меню: длинный Select **вне** меню (после cooldown 400 ms).

### Раскладка меню на LCD

- Строки 0–2: список из 3 параметров (`>label value`), прокрутка по `param_idx`.
- Строка 3 browse: `Sel=edit Hold=exit`
- Строка 3 edit: `Hold=ok L/R U/D    `
- Курсор в edit: колонка `MENU_VAL_COL` (7) + `digit_pos`.

### Типы параметров (`ParamType_t`)

| Тип | Поведение edit | Пример |
|-----|----------------|--------|
| `PTYPE_UINT` | поразрядное ±, delete/insert 0 | Zmax, Jdec |
| `PTYPE_CENTS` | целая + `.XX` | Zpit, sMin |
| `PTYPE_USTEP` | таблица 1,2,4,8,16,32 | ZuSt, XuSt |
| `PTYPE_BOOL` | toggle 0/1, LCD ON/OFF | Buzz, BlAu |

### Сохранение (`menu_save_all`)

Отложенное: `menu_pending_save` → save **после** `menu_exit()` (не блокировать UI).
Блоки EEPROM: `config_feed_save`, `config_machine_save`, `config_backlash_save` + `backlash_reload_steps`, `config_display_save`, `config_save` (buzzer).
После save: `ui_buttons_reset_joy()` — иначе ломается hold джойстика (см. `els-joy-feed`).

Выход из меню: `planner_stop_all()`, `ui_buttons_reset_joy()`, `motion_jog_resume()`.

## Единицы координат (CrdU)

| Значение | Константа | Флаг LCD | Отображение |
|----------|-----------|----------|-------------|
| 0 | `COORD_UNIT_STEPS` | `S` | `X 0001234` (7 цифр) |
| 1 | `COORD_UNIT_MM` | `M` | `X 12.345` (7 символов, trim leading zeros) |
| 2 | `COORD_UNIT_INCH` | `D` | конвертация из мм через pitch/steps |

EEPROM: magic `0xD1`, addr 70, checksum addr 72. Миграция со старого addr 64.

## Правила реализации

### ui_lcd

- `ui_lcd_set_line` — pad пробелами до 20 символов.
- `ui_lcd_set_line_raw` — ровно 20 байт (для выравнивания MPG/coords).
- Физический вывод — только `ui_lcd_update()` (loop каждые `LCD_UPDATE_MS` 80 ms + Timer5 ISR).
- Не вызывать `lcd.print` напрямую вне `ui_lcd.cpp`.

### ui_menu — добавление параметра

1. `edit_*` static + `MENU_PARAM_COUNT++`.
2. Запись в `param_def[]` (label ≤5 символов, min/max).
3. `menu_param_ptr_u16/u8`, `menu_load_values`, `menu_save_all`.
4. Проверить scroll (3 видимых строки) и col курсора.

### Производительность

- Форматирование LCD — в loop (`update_lcd`), не в stepper ISR.
- `ui_lcd_update` в Timer5 — только flush буфера; без snprintf/EEPROM.

## Чеклист при правках

```
- [ ] LCD 20×4: строки 0–3, pad/raw по назначению
- [ ] Главный экран: mode/feed/sub, MPG, coords+markers
- [ ] CrdU: steps/mm/inch + flag S/M/D в col 19
- [ ] Меню: browse/edit, long Select 600 ms, cooldown 400 ms
- [ ] Новый param: def + ptr + load + save + count
- [ ] menu_save_all: все config_*_save; backlash_reload после Bl*
- [ ] После save/exit: ui_buttons_reset_joy
- [ ] ui_menu_is_active блокирует update_main_lcd
- [ ] Минимальный diff, один файл за раз
- [ ] ISR: без тяжёлой логики и EEPROM
```

## Типичные ошибки

1. **Прямой LiquidCrystal вне ui_lcd** — ломает буфер и курсор.
2. **Save внутри menu_exit до сброса UI** — дёргает jog; использовать `menu_pending_save`.
3. **Забыть reset_joy после save** — джойстик «залипает» после EEPROM (pressing vs read).
4. **Label >5 символов** — не влезает в `%-5s` формат строки меню.
5. **Курсор edit вне видимых 3 строк** — проверить scroll и `cursor_row`.
6. **coords без set_line_raw** — сбивание выравнивания X/Z полей.
7. **CrdU без config_display_save** — единицы не переживают reboot.

## Связанные навыки

- `els-limits` — маркеры `< > F B` в `lcd_format_axis_field`
- `els-rgi-mpg` — `motion_jog_get_hand`, строка MPG
- `els-joy-feed` — Feed, jog после меню
- `els-backlash` — параметры Bl* в меню, экран BL takeup

## Дополнительно

- Полная таблица параметров и API: [reference.md](reference.md)
- Аппаратура: `02_Документация/HARDWARE.md` (LCD 2004, кнопки 22–26)
- Константы: `config.h` — `LCD_COLS` 20, `LCD_ROWS` 4, `LCD_UPDATE_MS` 80
