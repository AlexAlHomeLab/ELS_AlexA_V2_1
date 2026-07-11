# Делительная головка — справочник по коду ELS AlexA V2.1

## Файлы

```
01_Исходный_код/ELS_AlexA_V2.1/
├── src/modes/mode_divider/
│   ├── mode_divider.cpp
│   ├── mode_divider.h
│   └── mode_divider_cfg.h
├── src/core/process/spindle_control.cpp   # spindle_get_count()
├── src/core/fsm/fsm_manager.cpp
├── ELS_AlexA_V2.1.ino                     # update_main_lcd / divider LCD
├── src/core/ui/ui_buttons.cpp
└── src/config/config_modes.h
```

## Целевой API mode_divider

```c
void mode_divider_enter(void);
void mode_divider_exit(void);
void mode_divider_process(void);           /* poll кнопок + обновление LCD */

void mode_divider_zero_reset(void);        /* Select: zero_count = spindle_get_count() */
void mode_divider_parts_up(void);          /* A++ */
void mode_divider_parts_down(void);        /* A-- */
void mode_divider_part_prev(void);         /* B--, wrap */
void mode_divider_part_next(void);         /* B++, wrap */

uint8_t mode_divider_get_a(void);
uint8_t mode_divider_get_b(void);
float mode_divider_get_angle_deg(void);    /* фаза 0…360 для стр.0 */
float mode_divider_get_r_deg(void);        /* отклонение для B */
void mode_divider_format_line1(char *buf, size_t n);  /* A B R */
void mode_divider_format_line2(char *buf, size_t n);  /* COU */
void mode_divider_format_angle_center(char *buf, size_t n); /* 000.00 */

uint8_t mode_divider_is_active(void);      /* FSM mode 6 */
```

Legacy (не использовать в новой логике): `mode_divider_next()`, `mode_divider_set_parts()`, `planner_add_move` из divider.

## Константы (mode_divider_cfg.h)

| Макрос | Значение |
|--------|----------|
| `DIVIDER_MIN_PARTS` | 1 |
| `DIVIDER_MAX_PARTS` | 99 |
| `DIVIDER_DEFAULT_PARTS` | 1 |

## Состояние

| Переменная | Тип | Описание |
|------------|-----|----------|
| `div_a` | `uint8_t` | A — частей (1…99) |
| `div_b` | `uint8_t` | B — текущая часть (1…A) |
| `div_zero_count` | `int32_t` | опорный счёт энкодера после Select |

## Формулы

```c
#define DIVIDER_TICKS_PER_REV  ((int32_t)SPINDLE_ENCODER_PPR * 2)

static int32_t div_count_rel(void) {
    return spindle_get_count() - div_zero_count;
}

/* угол для строки 0 (0…360) */
static float div_angle_deg(int32_t rel) {
    int32_t t = rel % DIVIDER_TICKS_PER_REV;
    if (t < 0) t += DIVIDER_TICKS_PER_REV;
    return (float)t * 360.0f / (float)DIVIDER_TICKS_PER_REV;
}

/* целевой угол части B (1-based) */
static float div_target_deg(uint8_t a, uint8_t b) {
    return 360.0f * (float)(b - 1) / (float)a;
}

/* R: нормализация в (-180, +180] */
static float div_normalize_r(float r) {
    while (r > 180.0f) r -= 360.0f;
    while (r <= -180.0f) r += 360.0f;
    return r;
}

static float div_r_deg(int32_t rel, uint8_t a, uint8_t b) {
    return div_normalize_r(div_target_deg(a, b) - div_angle_deg(rel));
}
```

### COU

```
revs  = count_rel / DIVIDER_TICKS_PER_REV     /* int32_t, знак */
phase = count_rel % DIVIDER_TICKS_PER_REV
if (phase < 0) phase += DIVIDER_TICKS_PER_REV
/* phase → 4 цифры: (phase * 10000) / DIVIDER_TICKS_PER_REV  или тики напрямую — зафиксировать при реализации */
```

Пример snprintf:

```c
snprintf(buf, 21, "A %02u B %02u R%c%03u.%02u",
         div_a, div_b,
         r >= 0 ? '+' : '-',
         (unsigned)(fabsf(r)), (unsigned)(fabsf(r) * 100) % 100);

snprintf(buf, 21, "COU %c%07ld:%04ld",
         revs < 0 ? '-' : '+', (long)labs(revs), (long)phase4);
```

## LCD (целевая раскладка)

```
Строка 0: [Mode][ 123.45 ][Sub]     /* угол вместо Feed по центру */
Строка 1: A 36 B 12 R+002.50
Строка 2: COU -0000123:0450
Строка 3: X 12.345<Z -0.001>       /* стандарт */
```

Интеграция: в `update_lcd()` при `fsm_manager_get_mode() == 6` вызывать divider-форматирование для строк 0–2, строка 3 — общая.

## Кнопки (режим 6, меню закрыто)

| Кнопка | Действие |
|--------|----------|
| Up | A++ (max 99), clamp B |
| Down | A-- (min 1), clamp B |
| Left | B--, wrap |
| Right | B++ , wrap |
| Select | `mode_divider_zero_reset()` |
| Long Select | `ui_menu` open |

## Статус реализации

| Элемент | Статус |
|---------|--------|
| FSM enter/exit/process | есть (process stub) |
| LCD A/B/R/COU/угол | **нет** |
| Up/Dn/L/R / Select | **нет** |
| `spindle_get_count` + zero_count | **нет** |
| `mode_divider_cfg` 1…99 | **обновить** |
| Legacy planner/next | заготовка, **удалить при реализации** |

## FSM

```c
// fsm_manager.cpp — index 6 (селектор Pos8)
case 6: mode_divider_enter(); break;
case 6: mode_divider_exit(); break;
case 6: mode_divider_process(); break;
```

## UI константы

| Константа | Файл | Значение |
|-----------|------|----------|
| `MENU_SEL_HOLD_MS` | ui_menu.cpp | 600 ms — long Select → меню |
| `MENU_COOLDOWN_MS` | ui_menu.cpp | 400 ms |

## 7e2 (megaels) — аналог

- Угол с `Enc_Pos`, ручной поворот, без мотора.
- Up/Down = Total_Tooth (z), Left/Right = Current_Tooth (a).
- Select = `Enc_Pos = 0` (короткое нажатие).
- V2.1: те же принципы; другой формат LCD (2004, A/B/R/COU).
