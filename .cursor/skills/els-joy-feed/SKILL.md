---
name: els-joy-feed
description: >-
  Спецификация и реализация джойстика подач для ELS AlexA V2.1 на Arduino Mega 2560.
  Используй при разработке, отладке и ревью motion_jog, ui_buttons, ui_pot, planner, limits
  и FSM; когда пользователь упоминает джойстик подач, joy, rapid, go_lim, latch, pot feed
  или ручные подачи X/Z.
---

# Джойстик подач (ELS AlexA V2.1)

## Спецификация (ТЗ)

## Джойстик подач
Четыре положения (влево, вправо, вперед (верх), назад (низ))
Кнопка ускоренной подачи (rapid) включает максимальную настроенную скорость
При нажатии кнопки ускоренной подачи в движении включается ускоренная подача, при отпускании выключается. 
Скорости для разгона, подачи, торможения задаются в параметрах отдельно для каждой оси
Джойстик подач работает в ручном подрежиме, в автоциклах запускает цикл, прерывает цикл, возобновлет цикл.
Скорость подач устанавливается потенциометром.
Подачи ограничены лимитами.
Ускоренные подачи не ограничены лимитами
При удержании джойстика в одном положении и длинном нажатии левой кнопки лимита при включенном в направлении движения лимите срабатывает защелка и движение продолжается до лимита. 
Остановка движения любое включение джойстика

## Быстрый старт для агента

1. Прочитай [reference.md](reference.md) — карта файлов, API и поток данных.
2. Правь **только** файлы джойстика/jog/limits/FSM; не трогай несвязанные режимы без запроса.
3. Перед правками: кратко опиши **что** и **зачем**; жди подтверждения при нетривиальной логике.
4. После правок пройди чеклист ниже.

## Карта кода (кратко)

| Слой | Файл | Роль |
|------|------|------|
| HAL | `hal_pins.h` | `JOY_*_PIN` A8–A12, Port K |
| UI | `ui_buttons.cpp` | опрос джойстика, GO_LIM, latch, rapid+limit |
| UI | `ui_pot.cpp` | `ui_pot_get_jog_mm_min()` — скорость с потенциометра |
| Логика | `motion_jog.cpp` | `motion_jog_joy_poll()`, go_lim, `jog_clamp_target` |
| Движение | `planner.cpp` | `planner_exec_jog(..., "JOY", cruise=1)` |
| Конфиг | `config_machine.cpp` | max/rapid speed, feed_accel, jog_decel per axis/global |
| FSM | `fsm_manager.cpp` | `motion_jog_joy_poll()` только в `STATE_MANUAL` |
| Лимиты | `limits.cpp` | `limits_clamp_steps`, `limits_ui_go_target*` |

## Направления джойстика

| Кнопка | Пин | Ось | Знак |
|--------|-----|-----|------|
| Joy Left | A8 | Z | − |
| Joy Right | A9 | Z | + |
| Joy Up | A10 | X | + (вперёд) |
| Joy Down | A11 | X | − (назад) |
| Joy Rapid | A12 | — | моментальная ускоренная подача |

Одновременно X и Z допустимы (диагональ). Для latch/GO_LIM — только одна ось (`joy_feed_dir`).

## Скорость и параметры движения

### Обычная подача (без Rapid)

- Скорость: `ui_pot_get_jog_mm_min()` — потенциометр, min 10 мм/мин, cap `config_get_max_speed_mm_min(axis)`.
- Разгон: `config_get_feed_accel(axis)` × 50 → мм/с² в planner/stepper_gen.
- Торможение: тот же `feed_accel`; дистанция run-out — `config_get_jog_decel_steps()` (Jdec, глобально).

### Rapid (удержание A12)

- Скорость: `config_get_rapid_speed_mm_min(axis)` — максимальная настроенная для оси.
- Включение **только пока кнопка нажата** (`btn.joy_rapid = read()`), не latch.
- Во время движения: нажал → rapid, отпустил → снова pot-speed.

### Per-axis в конфиге (EEPROM / меню)

| Параметр | Ось | Назначение |
|----------|-----|------------|
| Max speed | X, Z | cap pot |
| Rapid speed | X, Z | rapid джойстик |
| Feed accel | X, Z | разгон и торможение |
| Jog decel steps | глобально | мин. шаги торможения при отпускании (Jdec) |

## Правила реализации

### FSM и подрежимы

- **Ручной подрежим (`STATE_MANUAL`)**: `motion_jog_joy_poll()` — непрерывная подача chunk + lookahead.
- **Автоциклы (ext/int)**: джойстик управляет FSM:
  - MANUAL → CYCLE — запуск цикла (клик/включение джойстика в подрежиме ext/int).
  - CYCLE → PAUSED — прерывание (любое включение джойстика).
  - PAUSED → CYCLE — возобновление.
- В `STATE_CYCLE` / `STATE_PAUSED` jog-poll **не** вызывается — только FSM-логика цикла.
- E-Stop блокирует jog и go_lim.

### Лимиты

- Обычная подача: цель через `jog_clamp_target(axis, target, rapid=0)` → `limits_clamp_steps`.
- Rapid: `jog_clamp_target(..., rapid=1)` — **без** clamp; лимиты можно проскакивать.
- У лимита при обычной подаче: если chunk не помещается — `planner_jog_stop`, позиция не меняется.

### GO_LIM и latch

- **GO_LIM (rapid)**: rapid + клик кнопки лимита (или rapid+limit hold) → `motion_jog_go_limit(idx)` — rapid speed, стоп при любом джойстике.
- **Latch**: удержание джойстика в направлении + **длинное** нажатие **левой** кнопки лимита (`LIMIT_LEFT`, idx=0), если программный лимит активен в этом направлении → `motion_jog_go_limit_latch(idx)`:
  - pot-speed (не rapid), движение до лимита;
  - latch-arm: после отпускания джойстика движение продолжается;
  - **любое** последующее включение джойстика — `go_limit_stop()`.
- `go_lim_active` блокирует обычный jog и РГИ.

### Остановка движения

- **Любое** включение джойстика останавливает текущее движение:
  - go_lim (click или on) → `go_limit_stop`;
  - при новом нажатии оси во время busy/backlash → `planner_jog_stop`;
  - при отпускании всех направлений → `planner_jog_stop` (если не mpg_active).
- Rapid **не** отменяет движение — только меняет скорость на лету.

### Связь с РГИ

- Старт джойстика по оси → `hand_reset_axis(axis)` — сброс `hand_pos` РГИ и MPG-счётчика.
- РГИ блокируется, если джойстик держит ту же ось (`blk joy` в `motion_jog_poll`).
- При правках обоих модулей читай skill `els-rgi-mpg`.

### Связь с люфтом

- Старт джойстика по оси → через `hand_reset_axis` / planner — `backlash_sync_axis`.
- go_lim у лимита: snap в пределах backlash → sync.
- При правках читай skill `els-backlash`.

### Планировщик

- Jog: `planner_exec_jog(tx, tz, jog_speed_dual_mm_min(rapid), "JOY", cruise=1, ...)`.
- `cruise=1` — retarget без рывков при удержании (chunk каждые `JOY_STEP_MS` × `JOY_LOOKAHEAD`).
- Dual-axis: скорость = max(speed_X, speed_Z).

## Чеклист при правках

```
- [ ] Четыре направления: Left/Right=Z, Up/Down=X
- [ ] Rapid: read() моментально; rapid_speed per axis; без clamp лимитов
- [ ] Обычная: pot → jog_mm_min, cap max_speed; clamp лимитов
- [ ] feed_accel per axis; jog_decel (Jdec) при отпускании
- [ ] STATE_MANUAL: joy_poll; циклы: start/pause/resume через FSM
- [ ] go_lim rapid: стоп при любом джойстике
- [ ] latch: joy hold + LimL hold + лимит в направлении → до лимита
- [ ] Любое включение джойстика останавливает go_lim / latch-arm
- [ ] hand_reset_axis при старте оси; блок РГИ на той же оси
- [ ] ui_buttons: read() для удержания (не pressing после EEPROM save)
- [ ] Минимальный diff, одна зона за раз
- [ ] ISR: без тяжёлой логики
```

## Типичные ошибки

1. **pressing() для joy hold** — ломается после `menu_save_all`; использовать `read()`.
2. **Clamp при rapid** — нарушает ТЗ; `jog_clamp_target` только при `rapid=0`.
3. **Rapid как latch** — rapid должен сниматься при отпускании A12.
4. **joy_poll в STATE_CYCLE** — jog в цикле запрещён; только FSM pause/resume.
5. **Latch без проверки направления лимита** — `limits_ui_go_target_dir`.
6. **Не останавливать go_lim при joy** — любое включение джойстика = stop.
7. **Забыть hand_reset** — накопитель РГИ не сбрасывается при jog.

## Пробелы реализации (проверять при доработке)

1. FSM: joy → start/pause/resume цикла — API есть (`fsm_cycle_*`), wiring из ui_buttons/fsm может быть неполным.
2. `jog_decel_steps` — глобальный Jdec, не per-axis (если ТЗ требует per-axis decel — вынести в config).
3. Отдельный feed decel per axis — сейчас accel=decel из одного `feed_accel`.

## Дополнительно

- Карта API и поток данных: [reference.md](reference.md)
- Документация: `02_Документация/STAGE_2_2.md`, `04_Промты_для_Cursor/03-fsm-manager.md`
- Аппаратура: `02_Документация/HARDWARE.md` (JOY A8–A12, POT feed)
