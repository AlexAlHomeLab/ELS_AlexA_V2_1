# Джойстик подач — справочник по коду ELS AlexA V2.1

## Файлы

```
01_Исходный_код/ELS_AlexA_V2.1/
├── src/core/hal/hal_pins.h           # JOY_LEFT..RAPID A8-A12
├── src/core/ui/ui_buttons.cpp        # poll, GO_LIM, latch, feed_joy_*
├── src/core/ui/ui_buttons.h          # ButtonState_t.joy_*
├── src/core/ui/ui_pot.cpp            # ui_pot_get_jog_mm_min()
├── src/core/motion/motion_jog.cpp    # motion_jog_joy_poll, go_lim
├── src/core/motion/motion_jog.h      # motion_jog_go_limit*, go_limit API
├── src/core/motion/planner.cpp       # planner_exec_jog, planner_jog_stop
├── src/core/motion/limits.cpp        # limits_clamp_steps, limits_ui_go_target*
├── src/core/fsm/fsm_manager.cpp      # STATE_MANUAL → joy_poll
├── src/core/fsm/fsm_cycles.cpp       # fsm_cycle_pause/resume
├── src/config/config_machine.cpp     # rapid, accel, jog_decel
└── ELS_AlexA_V2.1.ino                # ui_buttons_poll, ui_pot_poll
```

## API ui_buttons

```c
void    ui_buttons_poll(void);
ButtonState_t ui_buttons_get_state(void);  /* joy_* = read() уровни */
uint8_t ui_buttons_feed_joy_click(void);   /* click любого направления */
uint8_t ui_buttons_feed_joy_on(void);      /* удержание любого направления */
void    ui_buttons_reset_joy(void);        /* после menu save */
```

## API motion_jog (джойстик)

```c
void motion_jog_joy_poll(void);              /* основной poll джойстика */
void motion_jog_go_limit(uint8_t idx);       /* idx 0..3, rapid к лимиту */
void motion_jog_go_limit_latch(uint8_t idx); /* latch до лимита */
```

## Константы motion_jog.cpp

| Константа | Значение | Назначение |
|-----------|----------|------------|
| `JOY_STEP_MS` | 20 | Период тика chunk |
| `JOY_LOOKAHEAD` | 8 | Множитель chunk (runway) |

## Состояние joy/go_lim

| Переменная | Назначение |
|------------|------------|
| `joy_x_on`, `joy_z_on` | Активные оси джойстика |
| `joy_tick_ms` | Время последнего chunk |
| `go_lim_active` | Движение к программному лимиту |
| `go_lim_latch` | 1 = latch-режим |
| `go_lim_joy_arm` | latch: 0 пока joy держат, 1 после отпускания |
| `go_lim_stop_joy` | Флаг одноразовой паузы после stop |

## Поток данных (обычный jog)

```
ui_buttons_poll → btn_state.joy_*
       ↓
motion_jog_joy_poll (STATE_MANUAL)
       ↓
joy_chunk(axis, rapid) × JOY_LOOKAHEAD → target
       ↓
jog_clamp_target (rapid=0 → limits_clamp_steps)
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
    mm_min = ui_pot_get_jog_mm_min();  // pot, min 10, cap max_speed
}

static int32_t joy_chunk(uint8_t axis, uint8_t rapid) {
    // шагов за JOY_STEP_MS от mm_min и STEPS_PER_MM
}
```

## GO_LIM / latch (ui_buttons)

```
Rapid click + limit pressed     → motion_jog_go_limit(idx)     [rapid speed]
Limit click + rapid held        → motion_jog_go_limit(idx)
Limit LEFT hold + joy in dir    → motion_jog_go_limit_latch()  [pot speed, latch, beep 40 ms]
```

Проверка направления: `limits_ui_go_target_dir(axis, sign, &lim_idx, &target)`.
Beep latch: `hal_buzzer_beep_ms(40)` внутри `motion_jog_go_limit_latch` при успешном старте.
## FSM

```c
// fsm_manager.cpp
if (st == STATE_MANUAL) {
    motion_jog_joy_poll();
    motion_jog_poll();  // РГИ — отдельный skill
}
```

Переходы цикла (ТЗ / 03-fsm-manager.md):

```
MANUAL → CYCLE   (запуск джойстиком в ext/int)
CYCLE  → PAUSED  (любое включение джойстика)
PAUSED → CYCLE   (возобновление)
```

API: `fsm_cycle_start`, `fsm_cycle_pause`, `fsm_cycle_resume`, `fsm_cycle_stop`.

## Планировщик — jog

`planner_exec_jog(..., cruise=1)`:
- `MOTION_FLAG_JOG_CRUISE` — retarget без полной остановки
- `planner_jog_stop()` — при отпускании джойстика / stop go_lim
- Accel: `config_get_feed_accel(axis) * ACCEL_MM_S2_SCALE` (50 мм/с² per level)

## Пины (hal_pins.h)

| Сигнал | Пин |
|--------|-----|
| JOY_LEFT | A8 |
| JOY_RIGHT | A9 |
| JOY_UP | A10 |
| JOY_DOWN | A11 |
| JOY_RAPID | A12 |
| LIMIT_LEFT | (см. hal_pins.h) |
| POT_FEED | A0 (ui_pot) |

## Отладка

Serial: `JOG/JOY`, `JOG/GOLIM`, `JOG/LATCH`, `UI/GOLIM`, `UI/LATCH`
LCD: строка Feed через `ui_pot_feed_format`
