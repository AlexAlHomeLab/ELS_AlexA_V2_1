# РГИ — справочник по коду ELS AlexA V2.1

## Файлы

```
01_Исходный_код/ELS_AlexA_V2.1/
├── src/core/ui/ui_encoder.cpp      # ISR hand_mpg_isr_step, hand_count
├── src/core/ui/ui_encoder.h        # peek/consume/reset API
├── src/core/ui/ui_switches.cpp     # mpg_axis, mpg_scale
├── src/core/motion/motion_jog.cpp  # motion_jog_poll, hand_pos, mpg_cmd
├── src/core/motion/motion_jog.h    # motion_jog_get_hand()
├── src/core/motion/planner.cpp     # planner_exec_jog, planner_jog_stop
├── src/core/motion/limits.cpp      # limits_clamp_steps
├── src/core/fsm/fsm_manager.cpp    # STATE_MANUAL → motion_jog_poll
├── src/core/hal/hal_interrupts.cpp # INT2 → hand_mpg_isr_step
└── ELS_AlexA_V2.1.ino              # LCD строка MPG, motion_jog_poll в loop
```

## API ui_encoder

```c
int32_t ui_encoder_peek_mpg_delta(void);   // delta без consume
int32_t ui_encoder_consume_mpg_tick(void); // +1 / -1 / 0 за вызов
void    ui_encoder_reset_mpg(void);        // сброс счётчика (с hand_reset_axis)
int32_t ui_encoder_get_mpg_pos(void);      // абсолютный счётчик
```

## API motion_jog

```c
void    motion_jog_poll(void);             // основной poll РГИ
int32_t motion_jog_get_hand(uint8_t axis); // hand_pos для LCD
void    motion_jog_reset_pos(uint8_t axis);
```

## Константы motion_jog.cpp

| Константа | Значение | Назначение |
|-----------|----------|------------|
| `MPG_MAX_TICKS` | 24 | Макс. тиков за один poll |
| `MPG_LOOKAHEAD` | 4 | Множитель runway |
| `MPG_IDLE_STOP_MS` | 80 | Пауза до финиша MPG-движения |

## config_mpg.h

| Константа | Значение | Назначение |
|-----------|----------|------------|
| `MPG_RAPID_MODE_LIVE` | 0 | Rapid+РГИ: 0.1 мм, движение сразу |
| `MPG_RAPID_MODE_APPROACH` | 1 | Точный подвод: цель при Rapid, ход при отпускании |
| `MPG_RAPID_MODE` | APPROACH | Активный режим (compile-time) |

### Скорости РГИ (мм/мин, compile-time)

| Константа | Режим |
|-----------|-------|
| `MPG_SPEED_X1_X/Z_MM_MIN` | 1 шаг/тик |
| `MPG_SPEED_001_X/Z_MM_MIN` | 0.01 мм/тик |
| `MPG_SPEED_01_LIVE_X/Z_MM_MIN` | Rapid 0.1 мм, LIVE |
| `MPG_SPEED_APPROACH_X/Z_MM_MIN` | подвод APPROACH |

Рантайм: `mpg_speed_mm_min(axis, mpg_scale, rapid, approach)` в `motion_jog.cpp`.

## Состояние MPG в motion_jog.cpp

| Переменная | Назначение |
|------------|------------|
| `hand_pos[2]` | Накопитель шагов РГИ (X/Z) |
| `mpg_cmd[2]` | Целевая позиция в шагах от РГИ |
| `mpg_active` | Идёт MPG-движение |
| `mpg_axis_last` | Последняя ось РГИ |
| `mpg_last_ms` | Время последнего тика |

## Поток данных

```
INT2 (D18/D19) → hand_count (volatile)
       ↓
motion_jog_poll (STATE_MANUAL only)
       ↓
consume ticks → jog_steps_from_delta → mpg_cmd += steps
       ↓
limits_clamp → planner_exec_jog(tx,tz, mpg_speed_mm_min(...), "MPG", cruise=1)
       ↓
dds_motion (cruise retarget) → шаговые импульсы
```

## FSM

```c
// fsm_manager.cpp
if (st == STATE_MANUAL) {
    motion_jog_joy_poll();
    motion_jog_poll();  // ← единственная точка вызова РГИ
}
```

Циклы EXT/INT переводят FSM в `STATE_CYCLE` — РГИ не опрашивается.

## Селекторы (ui_switches)

- `mpg_axis`: AXIS_Z / AXIS_X (латч при LOW на соответствующем пине)
- `mpg_scale`: 0 = 1 шаг двигателя, 1 = 0.01 мм
- Rapid: `btn.joy_rapid` из `ui_buttons` → 0.1 мм/тик

## Планировщик — cruise

`planner_exec_jog(..., cruise=1)`:
- `MOTION_FLAG_JOG_CRUISE` — retarget без полной остановки
- `dds_motion_jog_retarget` — удлинение дистанции «на лету»
- При обгоне цели скоростью — одно непрерывное движение

## Пробелы реализации (проверять при доработке)

1. `MPG_REV_IGNORE_TICKS` (N) — логика игнора/стопа при смене направления
2. `hand_pos` при лимите — только фактические шаги
3. Отдельные `mpg_accel` / `mpg_decel` (если скорости по режимам потребуют другой профиль)

## Отладка

Serial debug-теги: `JOG/MPG`, `UI/AXIS`, `UI/SCALE`
LCD строка 1: `MPG X +123` / `MPG Z -45` через `motion_jog_get_hand()`
