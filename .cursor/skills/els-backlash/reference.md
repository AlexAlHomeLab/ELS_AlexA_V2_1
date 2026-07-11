# Компенсация люфта — справочник по коду ELS AlexA V2.1

## Файлы

```
01_Исходный_код/ELS_AlexA_V2.1/
├── src/config/config_defs.h         # ENABLE_BACKLASH, REF_DIR, FIRST_MOVE, defaults
├── src/config/config_backlash.h     # API настроек EEPROM
├── src/config/config_backlash.cpp   # BlAu/BlX/BlZ/BlSp/BlMn, comp_speed
├── src/core/motion/backlash.h       # публичный API модуля
├── src/core/motion/backlash.cpp     # rem_x/z, arm, consume, sync
├── src/core/motion/planner.cpp      # PLANNER_FLAG_BACKLASH, startup queue
├── src/core/motion/planner.h        # planner_add_backlash_takeup
├── src/core/motion/stepper_gen.cpp  # ISR: arm, consume, boost rates, bl_drain
├── src/core/motion/motion_control.cpp  # backlash_init, abort при stop
├── src/core/motion/motion_jog.cpp   # sync при hand_reset, GO_LIM
├── src/core/ui/ui_menu.cpp          # меню BlAu/BlX/BlZ/BlSp/BlMn
├── src/core/debug/serial_config.cpp # $40–$45
└── ELS_AlexA_V2.1.ino               # planner_startup_backlash_queue()
```

## API backlash

```c
void backlash_init(void);
void backlash_reload_steps(void);

void backlash_queue_takeup(uint8_t axis, uint8_t dir, int32_t steps);  /* планировщик */
void backlash_arm_axis(uint8_t axis, uint8_t new_dir, uint8_t enable); /* смена DIR */
uint8_t backlash_consume_step(uint8_t axis, uint8_t dir);              /* ISR: 1=съеден */

void backlash_sync_axis(uint8_t axis, uint8_t dir);   /* люфт взят */
void backlash_unsync_axis(uint8_t axis);              /* synced=0, rem=0 */
void backlash_abort_pending(void);                  /* rem=0, synced сохранён */

int32_t backlash_pending(uint8_t axis);               /* остаток rem */
uint8_t backlash_enabled(void);                       /* ENABLE_BACKLASH */
int32_t backlash_get_steps(uint8_t axis);
void backlash_set_steps(uint8_t axis, int32_t steps);

void backlash_log_poll(void);                         /* main: DBG_BL START */
```

## API config_backlash

```c
uint8_t  config_backlash_get_auto_on(void);
uint16_t config_backlash_get_steps_x(void);
uint16_t config_backlash_get_steps_z(void);
uint16_t config_backlash_get_auto_speed(void);   /* BlSp, мм/мин */
uint16_t config_backlash_get_min_speed(void);    /* BlMn, мм/мин */

float config_backlash_comp_speed_mm_min(uint8_t axis, float feed_mm_min);
/* clamp feed к [min_speed, auto_speed] и max_speed оси */
```

## API planner

```c
#define PLANNER_FLAG_BACKLASH  0x01U

int planner_add_backlash_takeup(uint8_t axis, float speed_mm_min);
void planner_startup_backlash_queue(void);
uint8_t planner_startup_busy(void);
```

## Внутреннее состояние backlash.cpp

| Переменная | Назначение |
|------------|------------|
| `backlash_steps_x/z` | Дистанция выборки (импульсы) |
| `rem_x/z` (volatile) | Остаток текущей выборки (ISR) |
| `last_dir_x/z` | Последнее физическое направление |
| `armed_dir_x/z` | Направление активной выборки |
| `synced_x/z` | 1 — люфт синхронизирован с механикой |

## Константы config_defs.h

| Константа | Значение | Назначение |
|-----------|----------|------------|
| `ENABLE_BACKLASH` | 1 | Условная компиляция |
| `BACKLASH_REF_DIR_X` | NEG | Направление стартовой выборки X |
| `BACKLASH_REF_DIR_Z` | NEG | Направление стартовой выборки Z |
| `BACKLASH_FIRST_MOVE` | ALWAYS | Первый ход → выборка |
| `BACKLASH_X_STEPS_DEFAULT` | 100 | BlX по умолчанию |
| `BACKLASH_Z_STEPS_DEFAULT` | 100 | BlZ по умолчанию |
| `BACKLASH_AUTO_DEFAULT` | 1 | BlAu по умолчанию |
| `BACKLASH_MIN_SPEED_DEFAULT` | 20 | BlMn, мм/мин |

## Поток данных

### Стартовая инициализация (BlAu=1)

```
setup → motion_init → backlash_init
loop (первый проход) → planner_startup_backlash_queue()
       ↓
planner_add_backlash_takeup(X/Z, comp_speed)
       ↓
PLANNER_FLAG_BACKLASH → dds_motion_start(MOTION_FLAG_BACKLASH)
       ↓
backlash_queue_takeup → rem=steps, DIR=REF
       ↓
ISR: consume_step × steps → backlash_sync_axis(REF)
```

### Смена направления (jog / MPG / цикл)

```
dds_motion_start / ISR step
       ↓
backlash_arm_axis(axis, new_dir, 1)
  if new_dir != last_dir → rem = steps
       ↓
профиль: distance += backlash_pending(master)
       ↓
ISR: step_pulse → backlash_consume_step
  rem>0: шаг без position++
  rem==0: last_dir=dir, synced=1
```

### Скорость при выборке

```
feed_mm_min (текущее движение)
       ↓
config_backlash_comp_speed_mm_min(axis, feed)  /* [BlMn..BlSp] */
       ↓
dds_motion_start → nominal_mm_min
       ↓
motion_boost_backlash_rates()  /* ISR: step_increment >= min(BlMn) */
```

## Serial ($n)

| $n | Параметр |
|----|----------|
| $40 | backlash auto (BlAu) |
| $41 | steps X (BlX) |
| $42 | steps Z (BlZ) |
| $43 | max speed mm/min (BlSp) |
| $45 | min speed mm/min (BlMn) |

После $41/$42: `backlash_reload_steps()`.

## Отладка

- `DBG_BL("START", axis, dir, steps)` — из `backlash_log_poll()` (очередь из ISR).
- `DBG_INFO_VAL("MOT", "BL", "Xstp/Zstp", ...)` — при init/reload.
- `DBG_PLN_ADD(bl=1, ...)` — backlash-блок в планировщике.

## EEPROM

- Адрес: 56 (`EEPROM_BACKLASH_ADDR`), checksum: 64.
- Magic: `0xB2`.
