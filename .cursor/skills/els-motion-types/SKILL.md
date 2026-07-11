---
name: els-motion-types
description: >-
  Таксономия возможных движений ELS AlexA V2.1: виды дистанций (шаг, подъём, треугольник,
  трапеция), уровни скорости (минимальная, подача, максимальная) и режимы (РГИ, джойстик,
  go_lim). Используй при отладке профиля движения, planner, stepper_gen, motion_jog и при
  вопросах о разгоне, торможении, cruise, retarget или классификации перемещений.
---

# Возможные движения (ELS AlexA V2.1)

## Спецификация (ТЗ)

##Возможные движения
У нас есть движения: шаг/несколько шагов рги меньше дистанции разгона, то же больше дистанции разгона. движение джойстиком со скоростью подачи, со скоростью быстрого перемещения, быстрое перемещение на дистанцию до лимита, перемещение на дистанцию со скоростью подачи. Виды дистанций: один шаг двигалеля, достanция меньше или равна дистанции разгона (подъем), дистанция меньше или равна дистанции разгона и торможения (треугольник), дистанция разгон, движение, торможение (полная трапеция). Скорости: ниже или равная минимальной, скорость подачи, максимальная скорость.

## Быстрый старт для агента

1. Определи **источник** (РГИ / джойстик / go_lim / цикл) и **класс дистанции** (см. ниже).
2. Прочитай [reference.md](reference.md) — матрица «движение × дистанция × скорость» и формулы.
3. Код профиля: `stepper_gen.cpp` → `motion_prep_profile()`, `motion_profile_extend()`, `accel_distance()`.
4. Код команд: `planner.cpp` → `planner_exec_jog()`, `planner_exec_axis()`; `motion_jog.cpp` → poll/go_lim.
5. Для деталей источника читай скилы `els-rgi-mpg`, `els-joy-feed`, `els-limits`.
6. Перед правками: кратко **что** и **зачем**; жди подтверждения при нетривиальной логике профиля.

## Виды дистанций

Расчёт в `motion_prep_profile(steps, nominal, initial, final, accel)` (`stepper_gen.cpp`):

| Класс (ТЗ) | Условие (master steps) | Поведение профиля |
|------------|------------------------|-------------------|
| **Один шаг двигателя** | `steps == 1` | Минимальный импульс; nominal/initial/final ≥ 1 sps |
| **Подъём** | `steps ≤ accel_dist(nominal, initial)` | Разгон не успевает выйти на nominal; пик ≤ nominal |
| **Треугольник** | `accel_dist + decel_dist ≥ steps` | Разгон + торможение без плато; `accelerate_until = decelerate_after = steps/2` |
| **Полная трапеция** | `accel_dist + decel_dist < steps` | Разгон → крейсер → торможение; плато между `accelerate_until` и `decelerate_after` |

Формула дистанции разгона/торможения (master-ось):

```
accel_dist = (v_hi² − v_lo²) / (2·a)     /* accel_distance() */
```

Фазы профиля: `motion_profile_phase()` — 0 разгон, 1 крейсер, 2 торможение.

**Jog cruise** (`MOTION_FLAG_JOG_CRUISE`): при удержании джойстика/РГИ `decelerate_after ≈ 0xFFFFFF00` — профиль без финального торможения между chunk'ами; retarget через `motion_profile_extend()`.

## Скорости

| Уровень (ТЗ) | Источник в коде | Примечание |
|--------------|-----------------|------------|
| **Ниже или равная минимальной** | `mm_min_to_sps()`: `< 1.0` → 1 sps; backlash `config_backlash_get_min_speed()` | Нижний порог DDS |
| **Скорость подачи** | `ui_pot_get_jog_mm_min()` (джойстик); `jog_speed_mm_min(axis, rapid=0)` (РГИ, latch go_lim) | Cap: `config_get_max_speed_mm_min(axis)` |
| **Максимальная скорость** | `config_get_rapid_speed_mm_min(axis)` (rapid, go_lim click) | Без clamp лимитов при rapid |

Разгон/торможение: `config_get_feed_accel(axis) × 50` → мм/с² → `axis_accel_steps_s2()`.

## Возможные движения (карта)

| Движение (ТЗ) | API / poll | cruise | Скорость | Типичный профиль дистанции |
|---------------|------------|--------|----------|----------------------------|
| РГИ: шаг / несколько шагов **< d_разг** | `planner_exec_jog(..., "MPG", cruise=1)` | да | подача | 1 шаг, подъём, треугольник |
| РГИ: **> d_разг** | то же + retarget `mpg_cmd` | да | подача | треугольник или трапеция; при серии импульсов — одно движение |
| Джойстик **скорость подачи** | `motion_jog_joy_poll()` → `"JOY", cruise=1` | да | pot feed | непрерывная трапеция/cruise; chunk каждые `JOY_STEP_MS` |
| Джойстик **быстрое перемещение** (Rapid) | то же, `jog_speed_dual_mm_min(rapid=1)` | да | rapid | cruise без clamp лимитов |
| **Быстрое перемещение на дистанцию до лимита** | `motion_jog_go_limit()` → `planner_exec_axis(..., rapid=1, jog=0)` | нет | rapid | трапеция/треугольник по расстоянию до лимита |
| **Перемещение на дистанцию со скоростью подачи** | `motion_jog_go_limit_latch()` → `planner_exec_axis(..., rapid=0, jog=0)` | нет | feed | финиш у лимита с торможением |

`d_разг` — `accel_distance(nominal, initial, accel)` для текущей оси и скорости подачи.

## Карта кода (кратко)

| Слой | Файл | Роль |
|------|------|------|
| Профиль | `stepper_gen.cpp` | `motion_prep_profile`, `motion_profile_extend`, ISR `stepper_generate_steps` |
| Планировщик | `planner.cpp` | `planner_exec_jog`, `planner_exec_axis`, `planner_jog_stop` |
| Jog | `motion_jog.cpp` | РГИ poll, joy poll, go_lim/latch |
| Конфиг | `config_machine.cpp` | max/rapid speed, feed_accel |
| UI | `ui_pot.cpp` | скорость подачи с потенциометра |

## Чеклист при отладке профиля

```
- [ ] Класс дистанции: steps vs accel_dist + decel_dist
- [ ] Скорость: min (1 sps) / feed (pot) / max (rapid)
- [ ] Источник: MPG cruise=1 vs JOY cruise=1 vs go_lim cruise=0
- [ ] РГИ короткий ход: triangle или 1 step, не лишний stop между тиками
- [ ] Joy/MPG длинный: retarget без рывка (motion_profile_extend)
- [ ] go_lim: полный профиль с торможением у цели
- [ ] Лимиты: clamp для feed/MPG; без clamp для rapid
- [ ] Лог: DBG_PLN_CRUISE / DBG_PLN_DECEL в debug_serial
```

## Типичные ошибки

1. **Стоп между тиками РГИ** — нарушает «одно движение»; нужен `cruise=1` + retarget.
2. **Clamp при rapid go_lim/joy** — rapid не должен ограничиваться лимитами.
3. **Путать треугольник и подъём** — подъём: steps ≤ accel_dist; треугольник: accel+decel ≥ steps.
4. **Торможение в joy hold** — при cruise финальное decel только при `planner_jog_stop` / отпускании.
5. **go_lim с cruise=1** — go_lim использует `planner_exec_axis(..., jog=0)`, не jog cruise.

## Связанные скилы

- РГИ: `els-rgi-mpg`
- Джойстик: `els-joy-feed`
- Лимиты / go_lim: `els-limits`
- Люфт (влияет на фактические шаги): `els-backlash`

## Дополнительно

- Матрица и формулы: [reference.md](reference.md)
