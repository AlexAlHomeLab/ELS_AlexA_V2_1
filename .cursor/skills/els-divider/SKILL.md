---
name: els-divider
description: >-
  Спецификация и реализация режима делительной головки (Divider) для ELS AlexA V2.1
  на Arduino Mega 2560. Используй при разработке, отладке и ревью mode_divider, FSM,
  LCD, spindle_control, энкодера шпинделя (INT0/PPR) и ui_buttons; когда пользователь
  упоминает делительную головку, divider, A/B/R/COU, энкодер шпинделя, INT0, PPR,
  деление окружности, части деления, отклонение угла или сброс угла.
---

# Делительная головка (ELS AlexA V2.1)

## Спецификация (ТЗ)

## Делительная головка
Меню:
Первая строка стандартно, в середине текущий угол поворота шпинделя 000.00
Вторая строка: A 002 B 001 R-000.00
Третья строка: COU -0000000:0000
Четвертая строка стандартно

Параметры: 
Текущий угол поворота сбрасывется в 0 нажатием Select
A количество частей деления, изменияется кнопками UP/DW, по умолчанию 2, минимум 2, максимум 999
B текущая часть деления, изменяется лево/право, по умолчанию 1, максимум не более A
R отклонение текщего положения угла от соотвествующего текущей части деления с учетом направления знак +/-
COU - текуще количество целых оборотов:фаза шпинделя, только отображение

Описание работы:
1. Установить количество частей деления
2. Сбросить текущее положение в 0 (Select)
3. Повернуть пока R не станет 0.
4. Указать следующую часть деления
повторить п 3.

## Быстрый старт для агента

1. Прочитай [reference.md](reference.md) — формулы, API, раскладка LCD, статус кода.
2. Правь `mode_divider/` и точки интеграции (`.ino` / LCD, `ui_buttons`); **не** planner/stepper для поворота шпинделя.
3. Угол, R, COU — из **энкодера шпинделя** (`spindle_get_count`), не из шагов оси X/Z.
4. Select в режиме 6 — **единственный** сброс опорного угла; long Select — меню EEPROM (`els-display-menu`).
5. `mode_divider_enter` / `exit` **не** сбрасывают A, B, `zero_count` (R) — настройки и ноль живут при уходе на другой режим и возврате.
6. Строки 0 и 3 — как главный экран; строки 1–2 — поля divider.

## Режим и FSM

| Параметр | Значение |
|----------|----------|
| Селектор | Pos8 → `sw.mode == 7` → FSM index **6** |
| Подрежимы | MAN / EXT / INT |
| Feed range | `FEED_RANGE_ASYNC` |
| Флаг | `ENABLE_MODE_DIVIDER` |

Вход/exit/process: `fsm_manager.cpp` case 6.

## LCD 2004 (режим Divider)

| Строка | Содержимое | Пример |
|--------|------------|--------|
| **0** | Стандартная status (`lcd_format_status_line`); **в центре** — угол шпинделя `000.00` | `[Div][ 123.45 ][Man]` |
| **1** | `A NNN B NNN R±000.00` | `A 360 B 120 R+002.50` |
| **2** | `COU ±NNNNNNN:PPPP` | `COU -0000123:0450` |
| **3** | Стандартные координаты X/Z + маркеры лимитов | как `update_main_lcd` |

Строка 1 — **20 символов**: `A %02u B %02u R%c%03u.%02u` (см. reference).

### Поля

- **Угол (стр. 0, центр):** `000.00`…`359.99` — фаза оборота относительно нуля после Select.
- **A:** 002…999, Up/Down (мин. 2 — одно деление бессмысленно).
- **B:** 001…A, Left/Right (wrap 1↔A).
- **R:** `+`/`-`, отклонение **фактического** угла от **целевого для части B**; оператор крутит шпиндель, пока R→0.
- **COU:** знак, 7 цифр целых оборотов, `:`, 4 цифры фазы — только LCD.

## Параметры и управление

| Параметр / действие | Кнопки | Диапазон | Default |
|---------------------|--------|----------|---------|
| **A** | Up / Down | 2…999 | 2 |
| **B** | Left / Right | 1…A | 1 |
| **Сброс угла** | **Select** (короткое) | — | `zero_count = spindle_get_count()`; A/B не трогать |
| **Меню EEPROM** | Long Select 600 ms | — | `ui_menu` (`els-display-menu`) |

При уменьшении **A** — **clamp B ≤ A**. Смена A не сбрасывает B автоматически (если B≤A — ок).

**Живучесть состояния:** `div_a`, `div_b`, `div_zero_count` — static, сохраняются при exit/enter FSM. Сброс угла **только** коротким Select в активном режиме 6. Смена селектора / `mode_divider_enter` **не** обнуляет угол и не возвращает A/B к default.

Джойстик/РГИ в MAN — по общим правилам; divider **не** двигает шпиндель автоматически.

## Энкодер шпинделя (HAL)

| Параметр | Значение |
|----------|----------|
| Пины | A = D20, B = D21 (INT0) |
| Прерывание | только **Rising** на B (`ISC01\|ISC00`); не Any Change |
| Тиков/оборот | `SPINDLE_ENCODER_PPR` (**×1**, не ×2 и не ×4) |
| Направление | в ISR по уровню A (`spindle_encoder_isr_step`) |
| Выкл. INT0 | `ENABLE_SPINDLE_ENCODER 0` в `config_machine.h` |
| RPM | `(delta * 60) / SPINDLE_ENCODER_PPR` в `spindle_poll` |

LCD пишется **после** `encoder_interrupts_init()`; шум/дребезг на D21 → лавина INT0 → битбанг HD44780 может «не стартовать» при живом Serial. Пинов с LCD нет.

## Формулы

```
TICKS_PER_REV = SPINDLE_ENCODER_PPR          /* Rising на B: ×1 к PPR (меню Spdl) */
count_rel     = spindle_get_count() - zero_count

angle_deg     = fmod(count_rel, TICKS_PER_REV) * 360.0 / TICKS_PER_REV   /* 0…360 для стр.0 */
target_deg    = 360.0 * (B - 1) / A                                     /* B — 1-based */
R_deg         = normalize(target_deg - angle_deg, (-180, +180])

COU_revs      = count_rel / TICKS_PER_REV          /* знаковое */
COU_phase     = abs(count_rel % TICKS_PER_REV)     /* 4 цифры, см. reference */
```

## Алгоритм (оператор)

1. Up/Down — задать **A**.
2. **Select** — сброс угла в 0.
3. Вручную повернуть шпиндель, пока **R = 0.00**.
4. Left/Right — следующая **B**.
5. Повторять п. 3.

## Карта кода

| Слой | Файл | Роль |
|------|------|------|
| Режим | `mode_divider.cpp` | A, B, zero_count, R/COU/angle, кнопки |
| Шпиндель | `spindle_control.cpp` | `spindle_get_count()`, ISR step |
| HAL INT | `hal_interrupts.cpp` | INT0 Rising на B; `ENABLE_SPINDLE_ENCODER` |
| LCD | `ELS_AlexA_V2.1.ino` | стр. 0/3 стандарт + divider стр. 1–2 |
| Кнопки | `ui_buttons.cpp` / divider | Up/Dn/L/R, Select → reset |
| FSM | `fsm_manager.cpp` | case 6 |

**Не использовать** для divider: `planner_add_move`, автоповорот оси (`mode_divider_next` — legacy).

## Правила реализации

- Сброс: `mode_divider_zero_reset()` → запомнить `zero_count`, **не** обнулять A/B.
- `mode_divider_enter()`: только `mode_active`, LCD dirty, sync бипера R — **без** записи A/B/`zero_count`.
- `mode_divider_exit()`: только `mode_active=0` (+ dirty LCD); состояние не чистить.
- Select в mode 6 обрабатывать **до** или **вместо** edit/browse меню (короткий клик).
- Long Select в divider — вход в `ui_menu`, как на главном экране.
- Форматирование LCD — в loop / `mode_divider_process`, не в ISR.

## Чеклист

```
- [ ] Стр. 0: status + угол 000.00 по центру
- [ ] Стр. 1: A 002 B 001 R±000.00
- [ ] Стр. 2: COU ±0000000:0000
- [ ] Стр. 3: coords + markers
- [ ] A: Up/Down 2…999; B: L/R 1…A, wrap; B≤A при смене A
- [ ] Select → zero_count = spindle_get_count(); A/B без изменений
- [ ] enter/exit не сбрасывают A, B, zero_count
- [ ] R для текущей B; нормализация ±180°
- [ ] COU только display
- [ ] Без planner/stepper для divider
- [ ] Форматирование в loop, не ISR
```

## Типичные ошибки

1. **Угол с шагов оси** — нужен энкодер шпинделя.
2. **Select открывает edit меню** — в mode 6 короткий Select = только сброс угла.
3. **R до следующей части** — R только для **текущей B**.
4. **Long Select для сброса** — сброс только **коротким** Select.
5. **planner_add_move** — не по ТЗ; ручной поворот шпинделя.
6. **Сброс A/B/zero в `enter`** — при возврате с другого режима настройки и R сбиваются; enter не должен их трогать.
7. **`TICKS_PER_REV = PPR * 2`** — устарело (было Any Change на B); as-is = **PPR ×1** (Rising).

## Связанные навыки

- `els-display-menu` — layout стр. 0/3, long Select → меню
- `els-rgi-mpg` — строка MPG заменена на A B R в divider
- `els-joy-feed` — jog в MAN
- `els-divider-thread-align` — **перспектива**: COU как ΔZ при подхвате резьбы

## Дополнительно

- [reference.md](reference.md)
- [els-divider-thread-align](../els-divider-thread-align/SKILL.md) — подхват резьбы, COU = дельта Z
- Референс логики угла (ручной поворот): 7e2 `Print.ino`, `Menu.ino` (megaels)
