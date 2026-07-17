# Селектор режимов — reference

## Дерево файлов

```
src/config/config_defs.h        # EN_X_SOFT_LATCH, EN_X_SETTLE_MS
src/config/config.h             # FIRMWARE_LCD_VER
src/core/hal/hal_pins.h         # EN_X_PIN, EN_Z_PIN, EN_X_* макросы
src/core/hal/hal_init.cpp       # pinMode EN_X при soft-latch
src/core/motion/motor_en.h/cpp  # ensure / release / latch
src/core/motion/stepper_gen.cpp # ensure перед X-движением
src/core/ui/ui_switches.*       # mode_off, mode_raw, poll release
src/core/motion/motion_jog.cpp  # blk MODE_OFF
ELS_AlexA_V2.1.ino              # update_mode_off_lcd()
```

## Пины

| Сигнал | Пин | Init |
|--------|-----|------|
| EN_X | D46 (PL3) | OUTPUT, LOW при soft-latch |
| EN_Z | D45 (PL4) | только `#define`, без pinMode |
| MODE_PIN_1..8 | D30..D37 | INPUT_PULLUP (как раньше) |

## API

```c
void motor_en_x_init(void);       /* из hal_init / motion_init */
void motor_en_x_ensure(void);     /* off→on + latch + settle */
void motor_en_x_release(void);    /* on→off, clear latch */
uint8_t motor_en_x_is_latched(void);

uint8_t ui_switches_mode_off(void); /* 1 = сырой скан 0 */
```

`SwitchState_t`: поле `mode_off` (1) и обычный `mode` (1–8 latch).

## Поток MODE_OFF

```
ui_switches_poll
  scanned = scan_mode_pin()   # digitalRead D30..D37, не EncButton
  mode_off = mode_off_debounced(scanned)  # OFF только после MODE_OFF_DEBOUNCE_MS
  scan>0 → mode_off=0 сразу; latch mode
  scan==0 устойчиво → mode_off=1, motion_jog_resume, motor_en_x_release
fsm_manager: использует sw.mode (latch), не mode_off
motion_jog_*: if mode_off → return
update_lcd: if mode_off → CNC OFF screen
```
Boot: mode_off=0, mode_off_raw_ms=0 (таймер только в poll).

Константа: `MODE_OFF_DEBOUNCE_MS` в `config_defs.h` (default 150).

## Связанные skills

- `els-joy-feed` — блок джойстика
- `els-rgi-mpg` — блок РГИ
- `els-display-menu` — форматирование LCD
- `els-changelog` — запись после смены функционала
