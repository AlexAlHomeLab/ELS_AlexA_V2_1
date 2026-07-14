---
name: els-limits
description: >-
  Спецификация и реализация программируемых лимитов (кнопки LimL/F/R/Re) для ELS AlexA V2.1
  на Arduino Mega 2560. Используй при разработке, отладке и ревью limits, ui_buttons,
  motion_jog, planner и LCD; когда пользователь упоминает лимиты, LimL, LimF, go_lim,
  LED лимита, обнуление оси, маркеры < > F B или clamp подач.
---

# Кнопки лимитов (ELS AlexA V2.1)

## Спецификация (ТЗ)

##Кнопки лимитов
Одно нажание установить лимит, второе нажатие сбросить лимит.
При установке лимита загорается светодиод подсветки кнопки, при сбросе гаснет.
Лимиты можно установить только в режиме асинхронной подачи, ручной подрежим.
Во всех остальных режимах лимиты не изменяются.
Лимиты устанавливает оператор по мере необходимости, хранить их после выключения станка не надо. 
Второй лимит может быть установлен если дистанция между первым лимитом и текущей позицией больше дистанции разгона+торможения
Долгое нажатие на лимит обнулят соотвествующую ось координат и аккумулятор рги. При этом физические позиции точек лимитов пересчитываются и сохраняются
При нажатии на rapid и кнопку лимита включается быстрая подача и движение до лимита. Любой клик или включение джойстиком останавливает это движение.
При нахождении суппорта на лимите справа от координаты на дисплее отображается соотвествующий символ <,>,F,B

## Быстрый старт для агента

1. Прочитай [reference.md](reference.md) — карта файлов, API и поток данных.
2. Правь **только** файлы лимитов/ui_buttons/motion_jog (clamp); не трогай несвязанные режимы без запроса.
3. Перед правками: кратко опиши **что** и **зачем**; жди подтверждения при нетривиальной логике.
4. После правок пройди чеклист ниже.
5. GO_LIM / latch / rapid — см. skill `els-joy-feed`; РГИ clamp и `hand_pos` — skill `els-rgi-mpg`.

## Карта кода (кратко)

| Слой | Файл | Роль |
|------|------|------|
| UI | `ui_buttons.cpp` | клик/hold лимита, rapid+limit → go_lim |
| Логика | `limits.cpp`, `limits.h` | установка/сброс, LED, clamp, LCD-маркер |
| Движение | `motion_jog.cpp` | `go_limit*`, `zero_axis`, `jog_clamp_target` |
| Движение | `planner.cpp` | `limits_clamp_steps` в целях planner |
| HAL | `hal_pins.h` | `LIMIT_*_PIN`, `LED_LIMIT_*_PIN` |
| LCD | `ELS_AlexA_V2.1.ino` | `limits_lcd_marker()` в строке координат |
| FSM | `fsm_manager.cpp` | `STATE_MANUAL` — jog/лимиты активны |

## Кнопки и оси

| idx | Кнопка | Ось | Направление | LCD-маркер |
|-----|--------|-----|-------------|------------|
| 0 | LimL | Z | min (−) | `<` |
| 1 | LimF | X | max (+) | `F` |
| 2 | LimR | Z | max (+) | `>` |
| 3 | LimRe | X | min (−) | `B` |

Пины: LimL 22, LimF 24, LimR 26, LimRe 28; LED 29/25/27/23 (active LOW).

## Правила реализации

### Установка и сброс (клик)

- **Первый клик** (лимит не активен): записать `limit_pos[idx]` = текущая позиция оси, `limit_active[idx]=1`, LED ON, beep.
- **Второй клик** (лимит активен): `limit_active[idx]=0`, LED OFF, beep.
- API: `limits_ui_on_click(idx)`; вызов из `ui_buttons` → `log_limit_event` при `btn.click()` без rapid.

### Допустимость установки (режим)

- ТЗ: только **режим Async** (`fsm_manager_get_mode() == 0`) + **ручной подрежим** (`SUB_MANUAL`) + **STATE_MANUAL**.
- В остальных режимах/подрежимах клик **не** меняет лимиты (go_lim/hold — отдельно, см. ниже).
- Проверять в `limits_ui_on_click` или в `ui_buttons` до вызова.

### Хранение

- Только RAM: `limit_active[4]`, `limit_pos[4]` в `limits.cpp`.
- `limits_ui_init()` при старте — все лимиты сброшены, LED OFF. **Без EEPROM.**

### Второй лимит на оси (min/max пара)

- Пара Z: LimL(0) + LimR(2); пара X: LimRe(3) + LimF(1).
- Обязательно: `min < max` (`can_set_limit`).
- ТЗ: дистанция между **уже установленным** лимитом и **текущей** позицией > **разгон + торможение** при текущей скорости подачи (pot / max_speed оси, `feed_accel`, `accel_distance` из stepper_gen или аналог в planner).
- При недостаточной дистанции — отказ установки (beep double или без изменения).

### Долгое нажатие (hold)

- Любая кнопка лимита hold → `limits_ui_on_hold(idx)` → `motion_jog_zero_axis(axis)`:
  - `limits_rebase_axis(axis, cur)` — физические точки лимитов сохраняются;
  - `motion_set_pos_steps(axis, 0)`;
  - `hand_pos[axis] = 0` (аккумулятор РГИ);
  - `ui_encoder_reset_mpg()`;
  - `backlash_sync_axis`.
- **Исключение LimL**: hold + джойстик в направлении активного лимита → latch (`motion_jog_go_limit_latch`) + короткий beep — см. `els-joy-feed`.

### GO_LIM (rapid + лимит)

- Rapid click при нажатой кнопке лимита **или** limit click при удержании rapid → `motion_jog_go_limit(idx)`.
- Rapid speed, **без** clamp лимитов по пути.
- Останов: **любой** клик или включение джойстика (`ui_buttons_feed_joy_click/on`).

### Ограничение движения (clamp)

- Обычная подача и РГИ: `limits_clamp_steps(axis, target)` → `[limits_get_min, limits_get_max]`.
- Неактивный край: `LIMIT_OFF_MIN` / `LIMIT_OFF_MAX` (±2e6) — «без лимита».
- Rapid go_lim: clamp **не** применяется к траектории.

### LCD-маркер

- `limits_lcd_marker(axis)`: если `pos == limit_pos[idx]` для активного лимита — `'<'` `'>'` `'F'` `'B'`, иначе `' '`.
- Вывод справа от координаты в `lcd_format_coords_line` (`.ino`).

### Аппаратные концевики

- Отдельно: `limits_hardware_check()` — `LIMIT_LEFT/RIGHT/UP/DOWN` (не путать с кнопками LimL/F/R/Re).

## Чеклист при правках

```
- [ ] Клик: toggle limit_active, LED, limit_pos = текущая позиция
- [ ] Установка только Async + SUB_MANUAL + STATE_MANUAL
- [ ] Без сохранения в EEPROM; limits_ui_init при boot
- [ ] Второй лимит: min<max И dist > accel+decel
- [ ] Hold: zero axis + rebase лимитов + hand_pos + mpg reset
- [ ] LimL hold + joy → latch (не zero), если лимит в направлении
- [ ] Rapid+limit → go_lim; стоп при joy click/on
- [ ] clamp в jog/MPG; rapid go_lim без clamp
- [ ] LCD: < > F B при pos == limit_pos
- [ ] Минимальный diff, одна зона за раз
```

## Типичные ошибки

1. **Сохранение лимитов в EEPROM** — нарушает ТЗ; только RAM.
2. **Установка лимита в Sync/Thread/цикле** — нужна проверка mode/submode/state.
3. **can_set_limit только min<max** — забыта проверка dist accel+decel для второго лимита.
4. **Hold всегда zero** — LimL+joy+лимит в направлении = latch, не zero.
5. **rebase при zero** — без `limits_rebase_axis` лимиты «уплывут» относительно станка.
6. **Путать кнопки Lim* и аппаратные LIMIT_* концевики** — разные пины и функции.
7. **LCD-маркер при pos≈limit** — сейчас строго `==`; при доработке не ломать точное совпадение без запроса.

## Пробелы реализации (проверять при доработке)

1. **Gating по режиму**: `limits_ui_on_click` вызывается без проверки Async/SUB_MANUAL — добавить по ТЗ.
2. **Дистанция разгон+торможение**: `can_set_limit` проверяет только порядок min/max, не run-out.
3. **Hold на всех Lim***: zero работает; latch только LimL — соответствует `els-joy-feed`.

## Дополнительно

- Карта API и поток данных: [reference.md](reference.md)
- Джойстик GO_LIM/latch: skill `els-joy-feed`
- РГИ у лимита: skill `els-rgi-mpg`
- Документация: `02_Документация/STAGE_2_2.md` (п. 2.2d), `HARDWARE.md`
- Аппаратура: кнопки D22–D28, LED D23–D29
