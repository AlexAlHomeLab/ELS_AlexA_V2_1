# Джойстик подач — справочник по коду ELS AlexA V2.1

Полная логика вариантов A–I и инварианты — в [SKILL.md](SKILL.md).

## Файлы

```
01_Исходный_код/ELS_AlexA_V2.1/
├── src/core/hal/hal_pins.h           # JOY_LEFT..RAPID A8-A12
├── src/core/ui/ui_buttons.cpp        # poll, GO_LIM, latch, feed_joy_*
├── src/core/ui/ui_buttons.h          # ButtonState_t.joy_*
├── src/core/ui/ui_pot.cpp            # ui_pot_get_jog_mm_min()
├── src/core/motion/motion_jog.cpp    # motion_jog_joy_poll, go_lim, runway
├── src/core/motion/motion_jog.h      # motion_jog_go_limit*, go_limit API
├── src/core/motion/planner.cpp       # planner_exec_jog, planner_jog_stop
├── src/core/motion/limits.cpp        # limits_clamp_steps, limits_ui_go_target*
├── src/core/fsm/fsm_manager.cpp      # STATE_MANUAL → joy_poll
├── src/core/fsm/fsm_cycles.cpp       # fsm_cycle_* (ТЗ; wiring joy→cycle нет)
├── src/config/config_machine.cpp     # rapid, accel, jog_decel
└── ELS_AlexA_V2.1.ino                # ui_buttons_poll, ui_pot_poll
```

## API ui_buttons

```c
void    ui_buttons_poll(void);
ButtonState_t ui_buttons_get_state(void);  /* joy_* = pressing() уровни */
uint8_t ui_buttons_feed_joy_click(void);   /* click любого направления */
uint8_t ui_buttons_feed_joy_on(void);      /* удержание любого направления */
void    ui_buttons_reset_joy(void);        /* после menu save */
```

## API motion_jog (джойстик)

```c
void motion_jog_joy_poll(void);              /* основной poll джойстика */
void motion_jog_go_limit(uint8_t idx);       /* idx 0..3, rapid к лимиту (D) */
void motion_jog_go_limit_latch(uint8_t idx); /* latch до лимита (E) */
```

## Константы motion_jog.cpp (joy-путь)

| Константа | Значение | Назначение |
|-----------|----------|------------|
| `JOY_STEP_MS` | 20 | Период проверки runway / retarget |
| `JOY_RUNWAY_REFRESH` | 8192 | Порог остатка цели → дозаправка |
| `JOY_RUNWAY_FILL` | 16000 | Длина runway в шагах |

`JOY_LOOKAHEAD` / `joy_chunk()` — не используются joy-poll (мёртвый код).

## Состояние joy/go_lim

| Переменная | Назначение |
|------------|------------|
| `joy_x_on`, `joy_z_on` | Активные оси джойстика |
| `joy_tick_ms` | Время последней проверки runway |
| `go_lim_active` | Движение к программному лимиту (D/E) |
| `go_lim_latch` | 1 = latch-режим (E) |
| `go_lim_joy_arm` | latch: 0 пока joy держат, 1 после отпускания |
| `go_lim_stop_joy` | После stop go_lim джойстиком: блок jog до отпускания (H) |

## Поток данных (обычный jog A/B)

```
ui_buttons_poll → btn_state.joy_* (pressing)
       ↓
motion_jog_joy_poll (STATE_MANUAL)
       ↓
остаток цели < JOY_RUNWAY_REFRESH?
  да → target = base ± JOY_RUNWAY_FILL
       ↓
jog_clamp_target (rapid=0 → limits_clamp_steps; rapid=1 → без clamp)
       ↓
planner_exec_jog(..., "JOY", cruise=1)
       ↓
dds_motion jog cruise retarget
```

## Скорость

```c
// motion_jog.cpp
static float jog_speed_mm_min(uint8_t axis, uint8_t rapid) {
    if (rapid) return config_get_rapid_speed_mm_min(axis);
    mm_min = ui_pot_get_jog_mm_min();  // pot; floor 10 (мм/об: 1); cap max_speed
}
```

Dual: `jog_speed_dual_mm_min` = max(speed_X, speed_Z).

## GO_LIM / latch (ui_buttons)

```
Rapid click + limit pressed     → motion_jog_go_limit(idx)     [D, rapid]
Limit click + rapid held        → motion_jog_go_limit(idx)     [D]
Limit hold + joy in dir         → motion_jog_go_limit_latch()  [E, pot, beep 40 ms]
Limit hold без joy              → limits_ui_on_hold (zero оси) [F]
```

Цель latch: `limits_ui_go_target_dir` — ближайший активный лимит оси впереди по sign.
Beep: `hal_buzzer_beep_ms(40)` в `motion_jog_go_limit_latch`.

## FSM

```c
// fsm_manager.cpp
if (st == STATE_MANUAL) {
    motion_jog_joy_poll();
    motion_jog_poll();  // РГИ — skill els-rgi-mpg
}
```

ТЗ (ещё не wired): MANUAL↔CYCLE↔PAUSED через joy.
API: `fsm_cycle_start`, `fsm_cycle_pause`, `fsm_cycle_resume`, `fsm_cycle_stop`.

## APPROACH (не joy)

Только: Rapid + тики РГИ + отпускание Rapid → подвод к `mpg_cmd`.
Joy+Rapid без тиков РГИ снимает arm. См. `els-rgi-mpg`.

## Планировщик — jog

`planner_exec_jog(..., cruise=1)`:
- `MOTION_FLAG_JOG_CRUISE` — retarget без полной остановки
- `planner_jog_stop()` — отпускание джойстика / после stop go_lim
- Чужие оси: `jog_peer_axis_hold` (target, не position)
- Accel: `config_get_feed_accel(axis) * ACCEL_MM_S2_SCALE` (50 мм/с² per level)

## Пины (hal_pins.h)

| Сигнал | Пин |
|--------|-----|
| JOY_LEFT | A8 |
| JOY_RIGHT | A9 |
| JOY_UP | A10 |
| JOY_DOWN | A11 |
| JOY_RAPID | A12 |
| LIMIT_* | (см. hal_pins.h) |
| POT_FEED | A0 (ui_pot) |

## Отладка

Serial: `JOG/JOY`, `JOG/GOLIM`, `JOG/LATCH`, `UI/GOLIM`, `UI/LATCH`, `JOG/MPG` (arm/arm joy)
LCD: строка Feed через `ui_pot_feed_format`
