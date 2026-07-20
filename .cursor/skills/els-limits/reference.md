# Кнопки лимитов — справочник по коду ELS AlexA V2.1

## Файлы

```
01_Исходный_код/ELS_AlexA_V2.1/
├── src/core/hal/hal_pins.h           # LIMIT_*_PIN, LED_LIMIT_*_PIN
├── src/core/ui/ui_buttons.cpp        # log_limit_event, rapid+limit
├── src/core/ui/ui_buttons.h          # ButtonState_t.limit_*
├── src/core/motion/limits.cpp          # limit_active, limit_pos, UI API
├── src/core/motion/limits.h            # публичный API
├── src/core/motion/motion_jog.cpp      # go_limit*, zero_axis, jog_clamp_target
├── src/core/motion/motion_jog.h
├── src/core/motion/planner.cpp         # limits_clamp_steps в целях
├── src/core/motion/stepper_gen.cpp     # accel_distance() — для проверки min dist
├── src/core/fsm/fsm_manager.cpp        # STATE_MANUAL → jog poll
├── src/config/config_machine.cpp       # feed_accel, rapid, max_speed
└── ELS_AlexA_V2.1.ino                # limits_ui_init, limits_lcd_marker на LCD
```

## API limits

```c
void limits_ui_init(void);
void limits_ui_on_click(uint8_t idx);   /* idx 0..3: toggle лимит */
void limits_ui_on_hold(uint8_t idx);    /* hold: zero оси через motion_jog_zero_axis */

uint8_t limits_ui_go_target(uint8_t idx, uint8_t *axis, int32_t *target);
uint8_t limits_ui_go_target_dir(uint8_t axis, int8_t sign, uint8_t *idx, int32_t *target);

uint8_t limits_ui_led_on(uint8_t idx);
char limits_lcd_marker(uint8_t axis);

void limits_rebase_axis(uint8_t axis, int32_t old_pos);

int32_t limits_get_min(uint8_t axis);
int32_t limits_get_max(uint8_t axis);
int32_t limits_clamp_steps(uint8_t axis, int32_t target);
uint8_t limits_is_active(uint8_t axis);
uint8_t limits_check(int32_t x_pos, int32_t z_pos);
uint8_t limits_hardware_check(void);
```

## Внутреннее состояние (limits.cpp)

| Переменная | Назначение |
|------------|------------|
| `limit_active[4]` | 1 — лимит установлен |
| `limit_pos[4]` | координата лимита, шаги |
| `LIMIT_OFF_MIN/MAX` | ±2000000 — «лимит не задан» |

## Индексы кнопок

```
idx 0  LimL   Z min   marker '<'
idx 1  LimF   X max   marker 'F'
idx 2  LimR   Z max   marker '>'
idx 3  LimRe  X min   marker 'B'
```

## Поток: установка лимита (клик)

```
ui_buttons_poll → btn_limit_*.click()
       ↓ (если !joy_rapid)
limits_ui_on_click(idx)
       ↓
toggle limit_active; limit_pos = motion_get_pos_steps(axis)
LIMIT_LED_ON/OFF; limit_beep()
```

## Поток: hold (обнуление / latch)

```
ui_buttons_poll → btn.hold()
       ↓
joy вкл (одно направление)?
  да → limits_ui_go_target_dir → motion_jog_go_limit_latch (zero нет)
  нет → limits_ui_on_hold(idx)
       ↓
motion_jog_zero_axis(axis)
       ↓
limits_rebase_axis(axis, cur)  /* limit_pos -= old_pos */
motion_set_pos_steps(axis, 0)
hand_pos[axis] = 0
ui_encoder_reset_mpg()
/* backlash не трогать */
```

## Поток: GO_LIM

```
rapid.click + limit pressing  → motion_jog_go_limit(idx)
limit.click + rapid read      → motion_jog_go_limit(idx)
       ↓
limits_ui_go_target → planner_exec_axis(..., rapid_speed)
       ↓
motion_jog_limit_poll: joy click/on → go_limit_stop()
```

## Поток: clamp (jog / MPG)

```
jog_clamp_target(axis, target, rapid=0)
       ↓
limits_clamp_steps(axis, target)
       ↓
clamp к [limits_get_min, limits_get_max]
```

## Пины (hal_pins.h)

| Сигнал | Пин |
|--------|-----|
| LIMIT_LEFT (LimL) | 22 |
| LIMIT_FRONT (LimF) | 24 |
| LIMIT_RIGHT (LimR) | 26 |
| LIMIT_REAR (LimRe) | 28 |
| LED_LIMIT_LEFT | 29 |
| LED_LIMIT_FRONT | 25 |
| LED_LIMIT_RIGHT | 27 |
| LED_LIMIT_REAR | 23 |

LED: `LIMIT_LED_ON` = LOW, `LIMIT_LED_OFF` = HIGH.

Аппаратные концевики (отдельно): `LIMIT_UP_PIN` = LimF, `LIMIT_DOWN_PIN` = LimRe.

## LCD

```c
// ELS_AlexA_V2.1.ino
lcd_format_axis_field(lcd_xf, 'X', xs, limits_lcd_marker(AXIS_X));
lcd_format_axis_field(lcd_zf, 'Z', zs, limits_lcd_marker(AXIS_Z));
```

Маркер только при **точном** совпадении `pos == limit_pos[idx]`.

## Проверка min dist (ТЗ, доработка)

Для второго лимита на оси:

```c
// Псевдокод — использовать feed_accel, pot/max speed, accel_distance()
dist = abs(cur_pos - existing_limit_pos);
runout = accel_distance(v_nom, v_entry, accel) + accel_distance(v_nom, v_exit, accel);
if (dist <= runout) reject;
```

Источник `accel_distance`: `stepper_gen.cpp` (static); при интеграции — вынести в общий helper или дублировать формулу из planner.

## Условие режима (ТЗ, доработка)

```c
fsm_manager_get_mode() == 0      /* Async */
&& fsm_manager_get_submode() == SUB_MANUAL
&& fsm_get_state() == STATE_MANUAL
```

## Отладка

Serial: `UI/BTN Lim*`, `UI/LIM off`, `UI/GOLIM`, `UI/HOLD`, `UI/LATCH`, `JOG/GOLIM`, `JOG/LATCH`, `MOT/LIM lcmp`
