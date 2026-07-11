# Reference: возможные движения ELS AlexA V2.1

## Формулы профиля

```
accel_dist(v_hi, v_lo, a) = (v_hi² − v_lo²) / (2·a)    /* шаги master-оси */

a_steps_s2 = feed_accel × 50 × steps_per_mm            /* axis_accel_steps_s2() */
nominal_sps = config_mm_min_to_sps(axis, speed_mm_min) /* cap max/rapid */
initial_sps = entry rate (0 при старте с покоя; текущая при retarget)
final_sps   = exit rate (0 для jog stop; entry для go_lim finish)
```

### Выбор класса дистанции

```
if steps == 1:
    → один шаг двигателя
elif steps <= accel_dist(nominal, initial, a):
    → подъём (только разгон, без выхода на nominal)
elif accel_dist(nominal, initial, a) + accel_dist(nominal, final, a) >= steps:
    → треугольник (пик в середине, без плато)
else:
    → полная трапеция (разгон → крейсер → торможение)
```

Реализация треугольника/трапеции — `motion_prep_profile()` строки с проверкой `accel_dist + decel_dist >= steps`.

---

## Матрица: движение × дистанция × скорость

### РГИ (MPG)

| Дистанция | Скорость подачи | Профиль | Код |
|-----------|-----------------|---------|-----|
| 1 шаг | feed | 1 step | `jog_steps_from_delta` → 1 step, `planner_exec_jog` |
| 2…N шагов, N ≤ d_разг | feed | подъём / малый треугольник | серия тиков, `mpg_cmd` накапливается |
| N > d_разг | feed | треугольник или трапеция | cruise retarget, без stop между тиками |
| любая (Rapid) | max (0.1 мм/тик) | как выше, выше nominal | `btn.joy_rapid` → scale 0.1 mm |

### Джойстик (JOY)

| Режим | Скорость | Дистанция | Профиль |
|-------|----------|-----------|---------|
| Удержание, feed | pot → feed | непрерывная (chunk + lookahead) | cruise: разгон один раз, далее плато + extend |
| Удержание, Rapid | rapid (max) | непрерывная | cruise без clamp лимитов |
| Отпускание | decel | `jog_decel_steps` (Jdec) | торможение до stop |

### Go to limit

| Режим | Скорость | Дистанция | Профиль |
|-------|----------|-----------|---------|
| GO_LIM (click) | rapid (max) | pos → limit | полный профиль, `jog=0` |
| Latch | feed | pos → limit | полный профиль с торможением у цели |

---

## API

| Функция | cruise | jog flag | Назначение |
|---------|--------|----------|------------|
| `planner_exec_jog(..., cruise=1)` | да | MOTION_FLAG_JOG | РГИ, джойстик |
| `planner_exec_jog(..., cruise=0)` | нет | MOTION_FLAG_JOG | короткий jog без cruise |
| `planner_exec_axis(..., jog=0)` | нет | 0 | go_lim, point-to-point |
| `planner_jog_stop()` | — | release cruise | отпускание джойстика / idle MPG |
| `dds_motion_jog_retarget()` | extend | retarget цели | удлинение без stop |

---

## Отладка

Логи (см. `debug_serial.cpp`):

- `[PLN] [CRUISE]` — вход в плато
- `[PLN] [DECEL]` — начало торможения
- `[JOG] GOLIM` / `LATCH` — go_lim

Проверять в ISR/main:

- `motion_prof.step_count`, `accelerate_until`, `decelerate_after`
- `motion_prof.jog_cruise` — 1 для JOY/MPG hold
- `motion_profile_phase()` — 0/1/2

---

## Связь с другими модулями

| Модуль | Влияние на профиль |
|--------|-------------------|
| `limits.cpp` | укорачивает цель → меньший steps → другой класс дистанции |
| `backlash` | доп. шаги выборки; в cruise не двигают `step_events` |
| `config_machine` | feed_accel, max/rapid speed → nominal и accel_dist |
