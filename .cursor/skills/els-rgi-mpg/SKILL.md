---
name: els-rgi-mpg
description: >-
  Спецификация и реализация РГИ (ручной генератор импульсов / MPG) для ELS AlexA V2.1
  на Arduino Mega 2560. Используй при разработке, отладке и ревью motion_jog, ui_encoder,
  planner, лимитов и FSM; когда пользователь упоминает РГИ, MPG, ручной генератор импульсов,
  hand_pos, mpg_scale, изоляцию РГИ, установку координат Left/Right/Up/Down+РГИ
  или точное ручное позиционирование.
---

# РГИ — ручной генератор импульсов (ELS AlexA V2.1)

## Спецификация (ТЗ)

## Ручной генератор импульсов (РГИ)
Выбор оси слектором оси x или z
Выбор цены импульса: селектором 1 импульс РГИ равен 1 импульсу двигателя, селектором 1 импульс РГИ равен 0,01 мм. В режиме LIVE нажатием кнопки ускоренной подачи 1 импульс РГИ равен 0,1 мм. В режиме точного подвода (APPROACH) Rapid не меняет цену импульса — шаг остаётся с селектора.
Параметры движения РГИ соотвествуют настройкам текущей оси и текущей  скорости подачи
Антидребезг импульсов РГИ
РГИ ограничен лимитами.
Если движение частичное (до лимита осталось меньше шага) - в аккумулятор идут только фактические шаги.
РГИ работает в ручном подрежиме всех режимов. 
РГИ в циклах, пузах циклов, после завершения цикла не работает.
При множестве импульсов в одну сторону они накапливаются (дистанция удлинняется) а движение не прерывается
Управление движением от РГИ идет стандартным способом через планировщик
Если движение еще не завершилось а РГИ сменил направление на N импульсов (случайный поворот, дребезг) то игнорируем, если более чем N то текущее движение немедленно полностью останавливается. N=3
Накопленные работой рги перемещения по каждой оси запоминаются в переменных. Сброс накопителей при подачи джойстиком
Если накопление дистанции от РГИ обгоняет скорость перемещения то это считается одним движением (нет торможения и остановки)

Нельзя проскакивать лимиты
Нельзя делать рывки и пропуски шагов

Работа РГИ **изолирована** от остальных функций (джойстик подач, go_lim/latch, меню, автоциклы, установка координат).
Предыдущие состояния и жесты **не влияют** на РГИ, **кроме** последних направлений движения по осям — они нужны только для выборки люфта (`els-backlash`).

## Установка координат кнопками дисплея + РГИ (ТЗ + as-is)

На главном экране в `STATE_MANUAL` (меню закрыто): удержание **одной** кнопки под LCD + РГИ —
правка отображаемой координаты **без хода**; отпускание всех L/R/U/D — commit. LCD: skill `els-display-menu`.

**Кнопки (as-is, `set_coord_poll`):**

| btn_id | Кнопка | Ось | Часть |
|--------|--------|-----|--------|
| 1 | Left | X | целая (1.000 / 1000 шагов) |
| 2 | Right | X | дробная (0.001 / 1 шаг) |
| 3 | Up | Z | **дробная** |
| 4 | Down | Z | **целая** |

`frac = (btn_id == 2 || btn_id == 3) ? 1 : 0` — Right и Up = дробная.

**As-is (реализовано):**

| Пункт | Реализация |
|-------|------------|
| Детект | `ui_buttons_set_coord_id()` — ровно одна L/R/U/D; `ui_buttons_set_coord_busy()` — любая нажата |
| Вход / блок MPG | любая L/R/U/D → discard+halt; arm при ровно одной |
| Release | только когда **все** L/R/U/D отпущены (`!busy`) |
| Тики | только `sc_pos`; planner / `mpg_cmd` при hold **не** крутят ось |
| Шаг тика | `set_coord_tick_steps(axis, sc_frac)` — CrdU мм/″/шаги; Xdia=D — `step/2` для мм/″ |
| Применение | `set_coord_apply_tick`: **не** через `val1000±1` (округление ломало уменьшение 0.001); всегда `± tick_sign * step` в машинных шагах |
| X радиус | `mpg_adjust_tick_sign` + инверсия X → `sc_pos += tick * step` |
| X диаметр | `sc_diameter_tick_sign` → `sc_pos -= tick * step` (крутим + → D на LCD растёт) |
| Z | `mpg_adjust_tick_sign` (Z invert compile-time) → `sc_pos += tick * step` |
| LCD | `motion_jog_set_coord_preview` → строка 4; X D через `lcd_x_steps_for_display` в `.ino` |
| Commit | `limits_rebase_axis`, `motion_set_pos_steps`, sync `mpg_cmd` |
| Cooldown | `SC_COOLDOWN_MS` (300 мс), продлевается при тиках энкодера |
| Блокировки | menu / MODE_OFF / go_lim / joy / не MANUAL |

Отладка INFO: `SC press/release/arm/tick/commit/cool`/`cool end`, `SC blk *`. Не спамить `SC pos` каждый тик.

## As-is: полярность РГИ (подача vs set-coord)

Две разные цепочки знака — **не объединять** в одну инверсию диаметра на `mpg_dir_lock`.

### `mpg_adjust_tick_sign(axis, raw ±1)`

- X: компенсация `config_get_dir_invert(AXIS_X)`.
- Z: опционально `MPG_AXIS_Z_INVERT` (compile-time).
- **Подача РГИ** (ход суппорта): в poll только эта функция → `jog_steps_from_delta` → `mpg_apply_tick` → `mpg_cmd`.
- `mpg_dir_lock` и реверс N=3 используют **тот же** знак, что и шаг подачи.

### `sc_diameter_tick_sign(axis, raw ±1)` — только set-coord, Xdia=D

- `mpg_adjust_tick_sign` + **доп.** инверсия для `X_COORD_MODE_DIAMETER`.
- Set-coord по D **не** совпадает по знаку с подачей РГИ на X: на LCD правим «номер D», подача — машинные шаги (как джойстик: вперёд → D на LCD уменьшается).

### Отображение Xdia=D (`.ino`, не motion)

- `lcd_x_steps_for_display`: машинные шаги → **−steps** на LCD и в строке `MPG D` (`hand_pos` тоже через минус).
- Джойстик вперёд увеличивает машинный X → D на экране **уменьшается** (инверсия только отображения + согласованный шаг мм в `jog_steps_from_delta`).

### Сводка знака (X, Xdia=D)

| Режим | Функция знака | Обновление позиции |
|-------|----------------|-------------------|
| РГИ подача | `mpg_adjust_tick_sign` | `mpg_cmd` += steps |
| Set-coord D | `sc_diameter_tick_sign` | `sc_pos -= tick * step` |
| Set-coord R (X) | adjust + invert X | `sc_pos += tick * step` |
| Set-coord Z | adjust (+ Z invert) | `sc_pos += tick * step` |
## Быстрый старт для агента

1. Прочитай [reference.md](reference.md) — карта файлов и API.
2. Правь **только** файлы РГИ/jog; не трогай несвязанные режимы и HAL без запроса.
3. Перед правками: кратко опиши **что** и **зачем**; жди подтверждения при нетривиальной логике.
4. После правок пройди чеклист ниже.

## Карта кода (кратко)

| Слой | Файл | Роль |
|------|------|------|
| ISR | `ui_encoder.cpp`, `hal_interrupts.cpp` | Счётчик `hand_count`, INT2 |
| UI | `ui_switches.cpp` | `mpg_axis` (X/Z), `mpg_scale` (1:1 / 0.01 мм) |
| UI | `ui_buttons.cpp` | `ui_buttons_set_coord_id()` — raw Port F для set-coord |
| Логика | `motion_jog.cpp` | `mpg_adjust_tick_sign`, `sc_diameter_tick_sign`, `set_coord_apply_tick`, poll |
| Движение | `planner.cpp` | `planner_exec_jog(..., "MPG", cruise=1)` |
| FSM | `fsm_manager.cpp` | РГИ/set-coord только при `STATE_MANUAL` |
| Конфиг | `config_mpg.h` | `MPG_RAPID_MODE`, скорости РГИ по режимам |
| LCD | `ELS_AlexA_V2.1.ino` | строка MPG, coords (Z|D), `lcd_x_steps_for_display`, превью set-coord |

## Масштаб импульса

Реализовано в `jog_steps_from_delta()` / `mpg_step_use_rapid()` (`motion_jog.cpp`):

| Условие | Шаг на 1 тик РГИ |
|---------|------------------|
| `mpg_scale == 0` | 1 шаг двигателя (Xdia не влияет) |
| `mpg_scale == 1` | `STEPS_PER_MM / 100` (0.01 мм радиуса) |
| Rapid + `MPG_RAPID_MODE_LIVE` | `STEPS_PER_MM / 10` (0.1 мм радиуса) |
| Rapid + `MPG_RAPID_MODE_APPROACH` | **как у селектора** (`mpg_scale`); Rapid не меняет шаг |

### Xdia = диаметр (только ось X, мм-масштабы)

Чтобы на LCD цена деления совпадала с селектором (0.01 / 0.1 **диаметра**), ход по радиусу **вдвое меньше**:

| Масштаб | Радиус (машина) | Отображение диаметра |
|---------|-----------------|----------------------|
| 0.01 мм | `spm/200` (0.005 мм) | +0.01 |
| Rapid 0.1 мм (только LIVE) | `spm/20` (0.05 мм) | +0.1 |

Z и x1step без изменений. См. также `els-display-menu` (Xdia).

### Режим Rapid + РГИ (`config_mpg.h`)

Compile-time: `MPG_RAPID_MODE` (по умолчанию `MPG_RAPID_MODE_APPROACH`).
**Только РГИ.** Джойстик Rapid (скорость / go_lim) APPROACH не использует и не армит.

| Режим | Поведение при Rapid + вращении РГИ |
|-------|-------------------------------------|
| `MPG_RAPID_MODE_LIVE` | 0.1 мм/тик, движение сразу (`planner_exec_jog`, cruise=1), лимиты не clamp |
| `MPG_RAPID_MODE_APPROACH` | шаг с селектора (x1 / 0.01); **только смена цели** (`mpg_cmd`); движение при **отпускании** Rapid (`cruise=0`, скорость APPROACH) |

- LCD: при нажатом Rapid строка MPG — `MPG>X …` / `MPG>Z …` (символ `>` после «MPG»).
- В APPROACH при наборе цели лимиты **clamp** (в отличие от LIVE).
- Шаг тика: `mpg_step_use_rapid()` — 0.1 только в LIVE; в APPROACH всегда `mpg_scale`.
- Фронт Rapid ↑: arm только если джойстик не держит подачу; иначе Rapid принадлежит JOY.
- Joy включился при arm без тиков РГИ → снять arm (не мешать джойстику).
- Фронт Rapid ↓: подвод к `mpg_cmd` только если были тики РГИ; иначе sync `mpg_cmd=pos` и стоп.
- Подвод: `cruise=1` до цели, по `pos==cmd` — `planner_jog_halt` (без coast/lead/bl_drain); флаг `mpg_approach_go`.
- Смена оси / halt: `mpg_rapid_prev=0` — при удержании Rapid повторный arm на новой оси.

Селекторы: `ui_switches` — `mpg_scale` 0 = x1step (D15 LOW), 1 = 0.01mm (D14 LOW).

## Скорости РГИ (`config_mpg.h`)

Отдельные compile-time константы **мм/мин** по оси X/Z; в рантайме `mpg_speed_mm_min()` clamp к `max_speed` оси. Джойстик и pot **не** используются для РГИ.

| Режим | Условие | Константы |
|-------|---------|-----------|
| 1:1 | `mpg_scale==0`, без Rapid | `MPG_SPEED_X1_X/Z_MM_MIN` |
| 0.01 мм | `mpg_scale==1`, без Rapid | `MPG_SPEED_001_X/Z_MM_MIN` |
| 0.1 мм LIVE | Rapid + `MPG_RAPID_MODE_LIVE` | `MPG_SPEED_01_LIVE_X/Z_MM_MIN` |
| Точный подвод | отпускание Rapid в APPROACH | `MPG_SPEED_APPROACH_X/Z_MM_MIN` |

Реализация: `mpg_speed_mm_min(axis, mpg_scale, rapid, approach)` в `motion_jog.cpp`; все `planner_exec_jog(..., "MPG", ...)` берут скорость оттуда.

## Правила реализации

### Инварианты изоляции (обязательны)

1. **Изоляция:** РГИ работает отдельно от джойстика подач, go_lim/latch, меню, автоциклов и set-coord. Не смешивать цели/сессии и не переносить жесты между подсистемами.
2. **Нет переноса состояний:** предыдущий жест (joy, go_lim, APPROACH arm, set-coord, смена режима) не влияет на текущую сессию РГИ — нет «хвоста» после чужого режима (после set-coord — cooldown 200 мс + discard тиков).
3. **Исключение — люфт:** единственная допустимая «память» между жестами/источниками — **последнее направление движения по оси** для выборки люфта (`backlash` / DIR). Подробности: `els-backlash`.
4. **Оси:** чужая ось при retarget — `jog_peer_axis_hold` (`target`, не `position`), чтобы не сбрасывать runway/сессию соседнего источника.
5. **Конфликт с joy:** если джойстик держит ось селектора РГИ — блок (`blk joy`); старт joy по оси → `hand_reset_axis`.

### Допустимость (FSM)

- РГИ активен **только** в `STATE_MANUAL` (подрежим MAN всех режимов 0–7).
- В `STATE_CYCLE`, паузах цикла, после завершения цикла — `motion_jog_poll()` не вызывается → РГИ не работает.
- E-Stop и `go_lim_active` блокируют РГИ.

### Накопление и планировщик

- Импульсы одного направления суммируются в `mpg_cmd[axis]`; цель удлиняется без промежуточных остановок.
- Каждый poll с новыми тиками → `planner_exec_jog` с `cruise=1` (единое движение без торможения между тиками).
- Цель planner = `mpg_cmd`, при живых тиках + pad **за cmd** (`mpg_cmd±lead`, не `pos+lead` — иначе runaway).
- Живой cruise: retarget; **не** stop+start при неудаче retarget (`planner.cpp`).
- Реверс: ≤N игнор, >N — `planner_jog_stop`, сброс сессии (ТЗ).
- `MPG_IDLE_MS` (2500): stop на pos; `dir_lock` после idle можно сменить после N.

### Накопитель `hand_pos`

- `hand_pos[AXIS_X/Z]` — накопленные шаги РГИ по оси (отображение на LCD: `MPG X/Z`).
- Сброс **только** при начале джойстика по этой оси (`hand_reset_axis`).
- Смена оси/масштаба РГИ накопитель **не** сбрасывает (STAGE_2_2e).
- При частичном движении до лимита в `hand_pos` добавлять **только фактически выполненные** шаги (разница `mpg_cmd` до/после `limits_clamp_steps`).

### Лимиты

- Все цели РГИ проходят `limits_clamp_target` / `limits_clamp_steps`.
- У лимита: если запрошенный шаг больше остатка — выполнить и учесть только достижимую часть.

### Антидребезг и смена направления

- ISR только инкрементирует `hand_count`; обработка в `loop` через `ui_encoder_consume_mpg_tick()` (по 1 тику).
- Пакетная обработка: до `MPG_MAX_TICKS` (24) тиков за один poll.
- ТЗ: реверс ≤N игнор, >N стоп (N=3). На практике при `mpg_active`/cruise — **только игнор** (дребезг детентов иначе рвёт «одно движение»).
- Смена направления — после `MPG_IDLE_MS` (`mpg_active=0`).
- `mpg_dir_lock` липкий на сессию.

### Параметры движения РГИ

- Скорость: `mpg_speed_mm_min()` — константы в `config_mpg.h` по режиму масштаба/Rapid/подвода; clamp к `max_speed` оси.
- Разгон/торможение — общие `feed_accel` оси (как у джойстика).
- Cruise-режим planner (`MOTION_FLAG_JOG_CRUISE`) обеспечивает «одно движение» при обгоне цели.

### Конфликт с джойстиком

- Любое удержание joy (X и/или Z) — РГИ блокируется (`blk joy`), coast/idle не идёт.
- Старт joy на оси → `joy_mpg_isolate_peer`: sync `mpg_cmd`/target на peer-оси.
- `jog_peer_axis_hold`: при joy ось без джойстика → `position`.
- См. инварианты изоляции выше; не дублировать логику joy внутри MPG-пути.

## Чеклист при правках

```
- [ ] РГИ только STATE_MANUAL, не в циклах
- [ ] Изоляция: нет хвоста от joy/go_lim/меню/set-coord; память только DIR люфта
- [ ] Масштаб: 1 step / 0.01 mm; 0.1 mm только Rapid+LIVE
- [ ] Xdia=диаметр, ось X: 0.01→0.005 мм, Rapid LIVE 0.1→0.05 мм радиуса; Z/x1 без изменений
- [ ] MPG_RAPID_MODE: LIVE (0.1) vs APPROACH (шаг с селектора); LCD `>` при Rapid
- [ ] APPROACH: Rapid не меняет шаг; подвод при отпускании
- [ ] Лимиты: clamp цели и фактические шаги в hand_pos
- [ ] Накопление mpg_cmd при серии импульсов
- [ ] cruise=1 в planner_exec_jog для MPG
- [ ] hand_pos сброс только при старте джойстика по оси
- [ ] Антидребезг: consume по 1 тику; reverse ≤N игнор, >N полный стоп
- [x] Отдельные скорости РГИ: `MPG_SPEED_*` в config_mpg.h, `mpg_speed_mm_min()`
- [x] Set-coord: Up/Down Z (Up дробная); шаг через `set_coord_tick_steps`; D — `sc_diameter_tick_sign`; не val1000±1
- [x] Подача РГИ X D: знак только `mpg_adjust_tick_sign`; dir_lock = знак подачи
- [x] LCD D: `-machine_steps`; строка 4: Z затем X/D
- [ ] Минимальный diff, одна зона за раз
- [ ] Производительность ISR: без тяжёлой логики в прерывании
```

## Типичные ошибки

1. **hand_pos += total_steps до clamp** — завышает накопитель у лимита; считать `actual = clamped - prev`.
2. **Сброс hand_pos при смене оси/масштаба** — нарушает STAGE_2_2e.
3. **Стоп между тиками РГИ** — ломает «одно движение»; нужен cruise + retarget.
4. **РГИ в STATE_CYCLE** — нарушает ТЗ; проверять FSM, не только motion_jog.
5. **Логика в ISR** — только `hand_mpg_isr_step()`, остальное в poll.
6. **0.1 мм/тик в APPROACH** — Rapid не меняет шаг; только `mpg_scale`. 0.1 — через `mpg_step_use_rapid()` лишь в LIVE.
7. **Перенос состояния с joy/go_lim** — нарушает изоляцию; после чужого режима сессия MPG должна быть чистой (кроме DIR люфта).
8. **Retarget чужой оси через position** — сбрасывает runway/сессию; `jog_peer_axis_hold`.
9. **Set-coord ждать EncButton hold 600 мс** — до таймаута MPG едет; детект с первого нажатия (raw Port F).
10. **Release по `id==0`** — дребезг/две кнопки дают ложный release → MPG с пальцем на кнопке; release только `!busy`.
11. **Фиксированный cooldown без тишины** — хвост тиков после окна стартует `MPG run`; продлевать, пока `peek!=0`.
12. **VERBOSE `SC pos` каждый тик** — спам Serial; только INFO на `arm`/`tick`(первый)/`commit`.
13. **LCD dirty без `sc_pos`** — превью set-coord не обновляется на экране.
14. **Инверсия D в `mpg_adjust_tick_sign` для подачи** — ломает `mpg_dir_lock` vs реальный шаг; D только в `sc_diameter_tick_sign` (set-coord) и на LCD.
15. **Set-coord дробная через val1000±1** — `sc_val1000_to_steps` часто не меняет `sc_pos` на ±0.001; только `set_coord_tick_steps`.
16. **Две инверсии dir_invert и D в одном tick_sign для подачи** — взаимно гасятся; подача без инверсии D, set-coord D — с `sc_diameter_tick_sign`.
## Дополнительно

- Пины и прерывания: [reference.md](reference.md)
- Установка координат X/Z: skill `els-display-menu`
- Люфт / DIR: skill `els-backlash`
- Джойстик подач: skill `els-joy-feed`
- Документация: `02_Документация/MODES.md`, `STAGE_2_2.md` (п. 2.2e)
- Аппаратура: `02_Документация/HARDWARE.md` (MPG_A/B, селекторы AXIS/RESOLUTION)
