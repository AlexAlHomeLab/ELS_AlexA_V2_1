---
name: els-backlash
description: >-
  Спецификация и реализация компенсации люфта (backlash) для ELS AlexA V2.1 на Arduino Mega 2560.
  Используй при разработке, отладке и ревью backlash, planner, stepper_gen, config_backlash;
  когда пользователь упоминает люфт, backlash, выборку, rem_x/rem_z, BlAu/BlX/BlZ или ENABLE_BACKLASH.
---

# Компенсация люфта (ELS AlexA V2.1)

## Спецификация (ТЗ)

##Компенсация люфта
функция выборки люфта по осям x и z.
Настройка дистанции выборки для каждой оси, в количестве импульсов.
Скорость выборки равна скорости текущего движения.
При первом включении оси инициализируются вращением на дистанцию выборки в направлениях указанных в настройках
Сделай возможность отключения функции компесации через условную компиляцию.
Выборка люфта при смене направления движения при работе РГИ, перемещение джойстиком, автоциклах.
Отключение двигателей через сигнал enable не сбрасывает состояние выборки люфта.
Компенсация идет через планировщик и встаривается о общую схему движения с разгонами и торможением. 
При этом для малых подач устанавливается минимальная скорость компенсации, что бы не ждать долго выборку люфта.

## Быстрый старт для агента

1. Прочитай [reference.md](reference.md) — карта файлов, API и поток данных.
2. Правь **только** файлы люфта/planner/stepper_gen; не трогай несвязанные режимы без запроса.
3. Перед правками: кратко опиши **что** и **зачем**; жди подтверждения при нетривиальной логике.
4. После правок пройди чеклист ниже.

## Карта кода (кратко)

| Слой | Файл | Роль |
|------|------|------|
| Ядро | `backlash.cpp`, `backlash.h` | `rem_x/z`, `synced`, `backlash_arm_axis`, `backlash_consume_step` |
| Конфиг | `config_backlash.cpp`, `config_defs.h` | EEPROM $40–$45, `ENABLE_BACKLASH`, REF-направления |
| Планировщик | `planner.cpp` | `PLANNER_FLAG_BACKLASH`, `planner_add_backlash_takeup`, стартовая очередь |
| DDS | `stepper_gen.cpp` | arm при смене DIR, consume в ISR, `motion_boost_backlash_rates` |
| Старт | `ELS_AlexA_V2.1.ino` | `planner_startup_backlash_queue()` после инициализации |

## Условная компиляция

В `config_defs.h`:

```c
#define ENABLE_BACKLASH  1   /* 0 — модуль отключён на этапе сборки */
```

При `ENABLE_BACKLASH=0`: `backlash_enabled()` → 0, шаги = 0, arm/consume — no-op. Код остаётся в проекте, но не активен.

## Настройки (EEPROM / меню / serial)

| Параметр | Меню | Serial | Назначение |
|----------|------|--------|------------|
| `auto_on` | BlAu | $40 | 1 — стартовая выборка при включении |
| `steps_x` | BlX | $41 | Дистанция X, импульсы (0 → из CENTIMM) |
| `steps_z` | BlZ | $42 | Дистанция Z, импульсы (0 → из CENTIMM) |
| `auto_speed` | BlSp | $43 | Макс. скорость выборки, мм/мин |
| `min_speed` | BlMn | $45 | Мин. скорость выборки, мм/мин |

REF-направление стартовой выборки: `BACKLASH_REF_DIR_X`, `BACKLASH_REF_DIR_Z` в `config_defs.h`.

## Правила реализации

### Дистанция выборки

- По оси X и Z отдельно: `backlash_steps_x/z` из EEPROM (`BlX`/`BlZ`) или из `BACKLASH_X/Z_CENTIMM` при steps=0.
- При смене направления: `backlash_arm_axis()` ставит `rem = steps`.
- Шаги выборки **не двигают** позицию: `backlash_consume_step()` возвращает 1 → позиция не инкрементируется.

### Скорость выборки

- Базовая: скорость **текущего** движения (`feed_mm_min` сегмента).
- Ограничение: `config_backlash_comp_speed_mm_min(axis, feed)` → clamp к `[BlMn, BlSp]` и `max_speed` оси.
- Для малых подач: `motion_boost_backlash_rates()` в ISR поднимает `step_increment` до минимума (`BlMn`), чтобы выборка не «ползла».

### Первое включение (инициализация осей)

- `BlAu=1` → `planner_startup_backlash_queue()` ставит блоки `PLANNER_FLAG_BACKLASH` по X и Z.
- Движение в `BACKLASH_REF_DIR_*` на `backlash_get_steps(axis)`; после завершения — `backlash_sync_axis`.
- `BACKLASH_FIRST_MOVE = BACKLASH_FIRST_ALWAYS` — первый ход после unsync тоже вызывает выборку.
- `planner_startup_busy()` блокирует FSM, пока стартовая очередь не опустеет.

### Выборка при смене направления

Работает единообразно для всех источников движения (джойстик, РГИ, автоциклы):

1. `dds_motion_start` / ISR шага вызывает `backlash_arm_axis(axis, new_dir, enable)`.
2. Если `new_dir != last_dir` и ось synced → `rem = steps`.
3. Если `!synced` и `BACKLASH_FIRST_ALWAYS` → выборка на первом движении.
4. Планировщик добавляет `bl_extra = backlash_pending(master)` к длине профиля разгона/торможения.

### Планировщик

- Отдельный блок: `planner_add_backlash_takeup(axis, speed)` с `PLANNER_FLAG_BACKLASH`.
- Позиция при backlash-блоке **не меняется**; DDS получает `MOTION_FLAG_BACKLASH`.
- Обычные сегменты: arm в `dds_motion_start`, consume в ISR — встроено в общий профиль.
- `planner_stop_all` / `motion_stop` → `backlash_abort_pending()` (отмена rem, **не** сброс synced/last_dir).

### Enable не сбрасывает состояние

- `dds_enable(axis, 0)` гасит только `axis_*.enabled` и `step_increment`.
- `rem_x/z`, `synced_x/z`, `last_dir_x/z` **сохраняются**.
- При повторном enable выборка продолжается с остатка `rem` или arm при новой смене DIR.
- Явный сброс: `backlash_unsync_axis`, `backlash_sync_axis`, `backlash_init`, `backlash_abort_pending`.

### Синхронизация (sync / unsync)

| Событие | Действие |
|---------|----------|
| Завершение backlash-блока планировщика | `backlash_sync_axis(axis, REF_DIR)` |
| Старт джойстика по оси (`hand_reset_axis`) | `backlash_sync_axis` — люфт «взят» |
| GO_LIM в пределах backlash | `backlash_sync_axis` |
| Hold zero оси (лимит) | **не** sync — только координата |
| `motion_set_zero` без BlAu | `backlash_sync_axis` обеих осей |
| E-Stop / полный стоп | `backlash_abort_pending` (rem=0, synced сохранён) |

## Чеклист при правках

```
- [ ] ENABLE_BACKLASH: сборка с 0 и 1
- [ ] BlX/BlZ: дистанция в импульсах, 0 → CENTIMM
- [ ] Скорость: feed текущего движения, clamp [BlMn, BlSp]
- [ ] Мин. скорость: motion_boost_backlash_rates при малых подачах
- [ ] Старт: BlAu → очередь REF_DIR, planner_startup_busy
- [ ] Смена DIR: arm для jog / MPG / циклов
- [ ] consume_step: позиция не меняется при rem>0
- [ ] enable OFF: rem/synced/last_dir не сбрасываются
- [ ] Планировщик: PLANNER_FLAG_BACKLASH, bl_extra в профиле
- [ ] Минимальный diff, одна зона за раз
- [ ] ISR: без тяжёлой логики; лог через bl_log_q → backlash_log_poll
```

## Типичные ошибки

1. **Инкремент position при rem>0** — нарушает компенсацию; только `backlash_consume_step`.
2. **Сброс synced при dds_enable(0)** — нарушает ТЗ; enable ≠ unsync.
3. **Фиксированная скорость выборки** — должна следовать feed с clamp к BlMn/BlSp.
4. **Выборка вне планировщика** — стартовая инициализация только через `planner_add_backlash_takeup`.
5. **Сброс last_dir при abort** — abort сбрасывает только rem; synced/last_dir остаются.
6. **Пропуск bl_extra в профиле** — движение завершается до конца выборки.

## Связь с РГИ

При правках РГИ + люфта читай также skill `els-rgi-mpg`. Точки пересечения:

- `backlash_arm_axis` в `dds_motion_start` при смене направления MPG.
- `hand_reset_axis` → `backlash_sync_axis` (не unsync).
- `planner_jog_stop` при pending backlash → `dds_motion_jog_release` / bl_drain.

## Дополнительно

- Карта API и поток данных: [reference.md](reference.md)
- Константы: `config_defs.h` (ENABLE_BACKLASH, REF_DIR, FIRST_MOVE, defaults)
- Документация: `02_Документация/ARCHITECTURE.md`
