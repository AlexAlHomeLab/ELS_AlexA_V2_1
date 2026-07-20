# Меню и дисплей — справочник по коду ELS AlexA V2.1

## Файлы

```
01_Исходный_код/ELS_AlexA_V2.1/
├── ELS_AlexA_V2.1.ino              # update_main_lcd, lcd_format_*
├── src/config/config.h             # LCD_COLS=20, LCD_ROWS=4, LCD_UPDATE_MS=80
├── src/config/config_display.cpp   # CrdU EEPROM
├── src/config/config_display.h
├── src/core/ui/ui_lcd.cpp          # LiquidCrystal, буфер, flush
├── src/core/ui/ui_lcd.h
├── src/core/ui/ui_menu.cpp         # меню, param_def, save
├── src/core/ui/ui_menu.h
├── src/core/ui/ui_buttons.cpp      # Select long/short, reset_joy
├── src/core/ui/ui_pot.cpp          # ui_pot_feed_format
├── src/core/hal/hal_timers.cpp     # таймеры ISR (LCD — не Timer5, см. els-lcd-libraries)
├── src/config/config_lcd.h         # USE_LCD_STANDARD | USE_LCD_SAFE_ASYNC
└── src/core/hal/hal_pins.h         # LCD_RS/EN/D4-D7, BTN_*
```

## API ui_lcd

```c
void ui_lcd_init(void);
void ui_lcd_update(void);                    /* flush буфера на HD44780 (только loop) */
void ui_lcd_process_queue(void);             /* SafeAsync: drain очереди; см. els-lcd-libraries */
void ui_lcd_set_line(uint8_t line, const char *text);   /* pad spaces */
void ui_lcd_set_line_raw(uint8_t line, const char *text); /* ровно LCD_COLS */
void ui_lcd_clear_line(uint8_t line);
void ui_lcd_clear(void);
void ui_lcd_set_cursor(uint8_t line, uint8_t col);
void ui_lcd_clear_cursor(void);
```

## API ui_menu

```c
void ui_menu_init(void);
void ui_menu_poll(void);           /* select long/short, up/down, edit digits */
uint8_t ui_menu_is_active(void);
void ui_menu_lcd(void);            /* отрисовка меню в буфер LCD */
```

## API config_display

```c
void config_display_load(void);
void config_display_save(void);
uint8_t config_get_coord_units(void);   /* COORD_UNIT_STEPS/MM/INCH */
void config_set_coord_units(uint8_t units);
char config_coord_unit_flag(void);      /* 'S' | 'M' | 'D' */
uint8_t config_get_x_coord_mode(void);  /* X_COORD_MODE_RADIUS/DIAMETER */
void config_set_x_coord_mode(uint8_t mode);
char config_x_coord_axis_char(void);    /* 'R' | 'D' */
```

## Константы ui_menu.cpp

| Константа | Значение | Назначение |
|-----------|----------|------------|
| `MENU_PARAM_COUNT` | 30 | число параметров |
| `MENU_EDIT_LEN` | 8 | буфер edit_buf |
| `MENU_COOLDOWN_MS` | 400 | пауза после open/exit/confirm |
| `MENU_SEL_HOLD_MS` | 600 | long Select |
| `MENU_VAL_COL` | 7 | колонка значения в строке меню |

## Таблица параметров меню

| idx | label | type | config / назначение |
|-----|-------|------|---------------------|
| 0 | Xdia | UINT | 0=радиус(R), 1=диаметр(D); `$47` |
| 1 | aMin | UINT | async feed min |
| 2 | aMax | UINT | async feed max |
| 3 | sMin | CENTS | sync feed min |
| 4 | sMax | CENTS | sync feed max |
| 5 | Zstp | UINT | Z motor steps |
| 6 | ZuSt | USTEP | Z microstep |
| 7 | Zpit | CENTS | Z screw pitch |
| 8 | Zmax | UINT | Z max speed mm/min |
| 9 | Zrap | UINT | Z rapid mm/min |
| 10 | Zacc | UINT | Z feed accel |
| 11 | Xstp | UINT | X motor steps |
| 12 | XuSt | USTEP | X microstep |
| 13 | Xpit | CENTS | X screw pitch |
| 14 | Xmax | UINT | X max speed |
| 15 | Xrap | UINT | X rapid |
| 16 | Xacc | UINT | X feed accel |
| 17 | Spdl | UINT | spindle PPR |
| 18 | Buzz | BOOL | buzzer on/off |
| 19 | Zinv | BOOL | Z dir invert |
| 20 | Xinv | BOOL | X dir invert |
| 21 | BlEn | BOOL | backlash enable |
| 22 | BlAu | BOOL | backlash auto |
| 23 | BlX | UINT | backlash X steps |
| 24 | BlZ | UINT | backlash Z steps |
| 25 | BlSp | UINT | backlash auto speed |
| 26 | BlMn | UINT | backlash min speed |
| 27 | Jdec | UINT | jog decel steps |
| 28 | CrdU | UINT | 0=steps, 1=mm, 2=inch |
| 29 | dEFt | ACTION | factory reset |

Формат строки списка: `"%c%-5s %s"` — маркер `>`, label 5 символов, value.

## Главный экран — форматирование (.ino)

```c
static void update_lcd(void) {
    if (ui_menu_is_active()) ui_menu_lcd();
    else update_main_lcd();
}

// Строка 0
lcd_format_status_line(mode_name, feed_txt, submode_short, buf);
// feed_txt ← ui_pot_feed_format(mode)

// Строка 1
lcd_format_mpg_line(axis, motion_jog_get_hand(mpg_axis));

// Строка 3
lcd_format_axis_field(lcd_xf, AXIS_X, xs, limits_lcd_marker(AXIS_X));
lcd_format_axis_field(lcd_zf, AXIS_Z, zs, limits_lcd_marker(AXIS_Z));
snprintf(buf, "%s%s", lcd_xf, lcd_zf);  // 10+10 = 20 cols
```

### Поле оси (10 символов)

Буква оси X = `config_x_coord_axis_char()` (`R`/`D`). В Xdia=диаметр: ×2 в числителе `lcd_steps_to_parts` до деления + round к 0.001 (квант 0.001 и для R, и для D).

**Steps:** `R 0071234<` или `R-0071234<` (шаги без ×2)

**mm/inch:** `R␠12.345␠<` — 7 символов числа, mark на pos 9.

### Feed (ui_pot.cpp)

```c
void ui_pot_feed_format(char *buf, size_t len, uint8_t mode);
// "F:120" (mm/min) или "F:0.15" (mm/rev) или "F:---" если pot не используется
```

## Поток данных

```
loop (каждый проход):
  ui_lcd_process_queue()   → [SafeAsync] drain очереди драйвера
  ui_menu_poll()           → open/edit/save/navigate
  lcd_mark_dirty_if_changed()
  if (lcd_dirty && 80 ms):
    update_lcd()           → ui_menu_lcd | update_main_lcd → lcd_buffer[][]
    ui_lcd_update()        → flush на HD44780 (только loop, не ISR)
```

Timer5 для LCD **не используется** — см. `els-lcd-libraries`.

## Select — state machine (ui_menu)

```
!menu_active:
  long Select → menu_open() [if cooldown]

menu_active + BROWSE:
  short Select click → menu_enter_edit()
  long Select → menu_pending_save=1, menu_exit()

menu_active + EDIT:
  long Select → menu_confirm_edit() → BROWSE
```

## EEPROM config_display

| Addr | Поле |
|------|------|
| 70 | magic `0xD2` |
| 71 | coord_units |
| 72 | x_coord_mode (Xdia) |
| 73 | checksum |

Миграция с magic `0xD1` (только CrdU): Xdia ← `X_COORD_MODE_DEFAULT`.

## Отладка

Serial: `UI/MENU open|exit`, `CFG/DSP loaded|saved|migr|defaults`; `$47` Xdia

## Проверка после правок

1. Главный экран: смена mode/submode, pot feed, MPG, coords при jog/MPG.
2. CrdU 0/1/2 — flag и формат чисел.
3. Xdia 0/1 — буква R/D; мм ×2 до /den + round 0.001; шаги без ×2; MPG X.
4. Меню: scroll 30 params (Xdia первый), edit, save → reboot.
5. Exit menu → jog/joy работает (reset_joy).

## Пробелы (ТЗ)

1. Установка текущих координат: hold Left/Right (X целая/дробная) или Up/Down (Z) + РГИ на главном экране; commit на отпускании; без хода. См. SKILL.md; wiring нет. Изоляция РГИ — `els-rgi-mpg`.
