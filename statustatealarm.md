# statustatealarm.md — таблица состояний ввода ELS AlexA V2.1

Документ моделирует **комбинации органов управления** при работе оператора **двумя руками** со станком.  
Источник: фактический код `01_Исходный_код/ELS_AlexA_V2.1/` (этап 2.2f), скилы проекта, read-only анализ 2026-07-11.

---

## 1. Как читать таблицу

### 1.1. Контекст (базовое состояние системы)

| Код | Контекст | Условие в коде |
|-----|----------|----------------|
| **CTX-00** | Нормальная работа | `STATE_MANUAL`, не menu, не startup, не estop, не go_lim |
| **CTX-01** | Startup backlash | `planner_startup_busy()` |
| **CTX-02** | Меню EEPROM | `ui_menu_is_active()` |
| **CTX-03** | GO_LIM / latch | `go_lim_active` |
| **CTX-04** | Divider UI | `fsm_manager_get_mode()==6`, не menu |
| **CTX-05** | FSM CYCLE | `fsm_get_state()==STATE_CYCLE` |
| **CTX-06** | FSM ERROR / E-Stop | `estop_is_triggered()` или `STATE_ERROR` |
| **CTX-07** | Смена режима | `fsm_manager_set_mode` в том же тике loop |

### 1.2. Органы ввода (две руки)

| Рука | Типичные органы | Пины (active LOW) |
|------|-----------------|-------------------|
| **Левая** | Joy L/R/U/D, Rapid, LimL/F/R/Re | A8–A12, D22/24/26/28 |
| **Правая** | MPG, ось X/Z, масштаб 1:1 / 0.01 мм | D18/19 INT2, D2/D3, D14/D15 |
| **Обе / любая** | Pot Feed, Mode Pos1–8, Sub Man/Ext/Int, LCD, шпиндель, E-Stop | A7, D30–37, A13–15, A0–A4, D20/21, D40 |

Порядок опроса в `loop()`: E-Stop → кнопки → селекторы → FSM poll → encoder → **меню** → **motion** → шпиндель → pot.

### 1.3. Вероятность (оценка для реальной работы на токарном ELS)

| Уровень | Обозначение | Смысл |
|---------|-------------|-------|
| **В** | Высокая | Штатная операция, >30% смены |
| **С** | Средняя | Регулярно, 5–30% |
| **Н** | Низкая | Редко, <5%, но осознанно |
| **Р** | Редкая/ошибка | Случайное сочетание, залипание, обучение |

### 1.4. Оценка учёта в коде

| Статус | Значение |
|--------|----------|
| **OK** | Поведение явно реализовано, соответствует ТЗ/скилу |
| **Частично** | Есть защита, но с дырой или побочным эффектом |
| **Нет** | Не обработано / мёртвый код / противоречие ТЗ |
| **Опасно** | Риск движения, ERROR, потери контекста, травмы |

---

## 2. Матрица приоритетов (кто побеждает при конфликте)

| Приоритет | Победитель | Проигравший | Файл |
|-----------|------------|-------------|------|
| 1 | **E-Stop** | всё | `estop_control.cpp`, `fsm_core.cpp` |
| 2 | **Startup BL** | jog, MPG, mode_divider process | `fsm_manager.cpp:107-112` |
| 3 | **go_lim** | joy chunk, MPG poll | `motion_jog.cpp:464-469, 578` |
| 4 | **Joy (та же ось)** | MPG | `motion_jog.cpp:673-676` |
| 5 | **Меню (LCD)** | Divider A/B/Select | `mode_divider.cpp:148` |
| 6 | **Rapid** | pot-speed, clamp лимитов | `jog_clamp_target` |
| 7 | **Селектор mode** | exit_mode → stop (не все режимы) | `fsm_manager.cpp`, `mode_*_exit` |

---

## 3. Таблица состояний — штатные сценарии (две руки)

| ID | Контекст | Комбинация ввода (левая + правая + прочее) | Вероят. | Последствия для станка | Учёт в коде | Ссылки |
|----|----------|---------------------------------------------|---------|------------------------|-------------|--------|
| **S01** | CTX-00 | Joy одна ось + Pot | **В** | Непрерывная подача по pot, clamp soft limits | **OK** | `motion_jog_joy_poll`, `ui_pot_get_jog_mm_min` |
| **S02** | CTX-00 | Joy диагональ X+Z | **С** | Одновременное движение, скорость max(X,Z) | **OK** (риск рывка при смене master) | `motion_jog.cpp:542-558`, `stepper_gen.cpp:855-859` |
| **S03** | CTX-00 | Joy Z + MPG X (разные оси) | **В** | Параллельные оси; MPG после joy в одном тике | **Частично** | `fsm_manager.cpp:124-125` |
| **S04** | CTX-00 | Joy Z + MPG Z (та же ось) | **С** | MPG блокируется, работает только joy | **OK** | `motion_jog.cpp:673-676` |
| **S05** | CTX-00 | Rapid удержание + Joy | **В** | Rapid speed, **без** clamp программных лимитов | **OK** (по ТЗ) | `jog_clamp_target(rapid=1)` |
| **S06** | CTX-00 | Rapid + MPG | **С** | Шаг 0.1 мм/тик, без clamp | **OK** | `jog_steps_from_delta` |
| **S07** | CTX-00 | Rapid клик + Lim* удержание | **Н** | GO_LIM rapid к программному лимиту | **OK** | `ui_buttons.cpp:151-157` |
| **S08** | CTX-00 | Lim* клик (без rapid) | **С** | Toggle программного лимита, LED | **Частично** (нет gating Async+MAN) | `limits.cpp:66-91` |
| **S09** | CTX-00 | LimL hold + Joy в сторону лимита | **Н** | Latch: подача до лимита после отпускания joy | **OK** | `ui_buttons.cpp:117-127` |
| **S10** | CTX-00 | Lim* hold (без latch-условия) | **Н** | Zero оси + rebase лимитов + сброс MPG | **OK** | `limits_ui_on_hold` → `motion_jog_zero_axis` |
| **S11** | CTX-00 | MPG серия импульсов одного направления | **В** | Накопление `mpg_cmd`, cruise без стопов | **OK** | `motion_jog_poll`, `MPG_REV_IGNORE_TICKS` |
| **S12** | CTX-00 | MPG реверс ≤3 тика на ходу | **С** | Игнор дребезга | **OK** | `motion_jog.cpp:702-704` |
| **S13** | CTX-00 | MPG реверс >3 тика на ходу | **Н** | Полный abort MPG/jog | **OK** | `mpg_motion_abort` |
| **S14** | CTX-00 | Смена оси MPG (Z↔X) во время MPG | **С** | `mpg_session_halt`, discard delta, arm 2 loop | **OK** | `motion_jog_on_axis_select` |
| **S15** | CTX-00 | Смена масштаба MPG (1:1 ↔ 0.01 мм) | **С** | Только лог; hand_pos не сбрасывается | **OK** | `ui_switches.cpp:179-182` |
| **S16** | CTX-00 | Pot вращение во время joy | **В** | Скорость меняется на лету (100 ms фильтр) | **OK** | `ui_pot.cpp`, `config.h:POT_READ_MS` |
| **S17** | CTX-00 | Mode Pos переключение во время движения | **С** | exit_mode: `planner_stop_all` у async/sync/thread/grind | **Частично** | `mode_async_exit` и др. |
| **S18** | CTX-00 | Sub Man/Ext/Int переключение | **С** | Только запись submode + LCD; **движение не меняется** | **Нет** (ТЗ ext/int не wired) | `fsm_manager_set_submode` |
| **S19** | CTX-04 | Divider: шпиндель вручную + Joy Z | **В** | Угол/R/COU на LCD; ось Z движется | **OK** | `mode_divider` + `motion_jog` |
| **S20** | CTX-04 | Divider: Up/Down + шпиндель | **С** | A меняется; R пересчитывается | **OK** | `mode_divider_process` |
| **S21** | CTX-04 | Divider: Select short | **С** | Сброс опорного угла шпинделя | **OK** | `mode_divider_zero_reset` |
| **S22** | CTX-04 | Divider: L/R (B) + кручение шпинделя до R≈0 | **В** | Бип long при R→0 | **Частично** (`delay` в loop) | `div_poll_r_beep` |
| **S23** | CTX-02 | Меню: Long Select → browse/edit | **С** | LCD меню; **jog/MPG не блокируются** | **Нет** | `ui_menu.cpp`, нет guard в `motion_jog_*` |
| **S24** | CTX-02 | Меню exit / save | **Н** | `planner_stop_all`, reset joy, `motion_jog_resume` | **OK** | `menu_exit`, `menu_save_all` |
| **S25** | CTX-01 | Startup BL + Joy | **С** | Jog/MPG заблокированы, лог «blk startup» | **OK** | `fsm_manager.cpp:107-112` |
| **S26** | CTX-03 | GO_LIM активен + Joy click/on | **С** | Останов go_lim | **OK** | `motion_jog_limit_poll:411-415` |
| **S27** | CTX-03 | Latch: joy отпущен → движение к лимиту | **Н** | Продолжение на pot-speed | **OK** | `go_lim_latch`, `go_lim_joy_arm` |
| **S28** | CTX-06 | E-Stop нажат | **Р** | FSM ERROR, motion_stop, PWM=0, бипер | **Частично** | `estop_control.cpp` |
| **S29** | CTX-06 | E-Stop reset через выход из меню | **Р** | `estop_reset` → MANUAL | **OK** (неочевидно оператору) | `ui_menu.cpp:606-608` |

---

## 4. Таблица состояний — конфликтные и аварийные сочетания

| ID | Контекст | Комбинация | Вероят. | Последствия | Учёт в коде | Alarm |
|----|----------|------------|---------|-------------|-------------|-------|
| **A01** | CTX-02 | Меню edit + Joy/MPG одновременно | **С** | Движение суппорта при правке Zmax/Bl*/CrdU | **Нет** | ALM-MENU-MOVE |
| **A02** | CTX-02 | Menu save (EEPROM) + удержание Joy | **Н** | Параметры машины меняются **во время** движения | **Частично** | ALM-MENU-MOVE |
| **A03** | CTX-03 | GO_LIM + только Rapid (без направления joy) | **Н** | GO_LIM **не останавливается** (ТЗ: любой joy) | **Нет** | ALM-GOLIM-RAPID |
| **A04** | CTX-03 | GO_LIM старт при `dds_motion_busy` | **С** | Тихий отказ старта (`planner_exec_axis` return) | **Нет** alarm | ALM-GOLIM-BUSY |
| **A05** | CTX-01 | Startup BL + Rapid+Lim → GO_LIM | **Р** | Возможное наложение на backlash queue в DDS | **Нет** | ALM-STARTUP-GOLIM |
| **A06** | CTX-06 | E-Stop → отпускание → menu exit без осознания | **Р** | `go_lim_active` может **пережить** E-Stop | **Опасно** | ALM-ESTOP-GOLIM |
| **A07** | CTX-00 | Joy/MPG/GO_LIM при нажатых Lim* как «концевиках» | **Н** | В jog **нет** `limits_hardware_check`; пины = UI | **Опасно** | ALM-HW-LIM-JOG |
| **A08** | CTX-05 | STATE_CYCLE + Joy | **С** | Jog не двигает (FSM gate), **pause/resume нет** | **Нет** | ALM-CYCLE-JOY |
| **A09** | CTX-05 | STATE_CYCLE + GO_LIM/latch | **Н** | Движение к лимиту «вне» логики цикла | **Частично** | ALM-CYCLE-GOLIM |
| **A10** | CTX-05 | Grind stop / cycle done → CYCLE→MANUAL | **Р** | **Невалидный переход** → STATE_ERROR | **Опасно** | ALM-FSM-INVALID |
| **A11** | CTX-07 | Смена Pos с Grind при CYCLE | **Р** | `mode_grind_exit` → IDLE из CYCLE → ERROR | **Опасно** | ALM-FSM-INVALID |
| **A12** | CTX-00 | Joy диагональ + latch LimL | **Р** | `joy_feed_dir`→0, latch **не сработает** | **OK** (защита) | — |
| **A13** | CTX-00 | Joy Z+X одновременно в `joy_feed_dir` | **Р** | Только для latch/go_dir — отказ | **OK** | — |
| **A14** | CTX-04 | Divider Select long (~600 ms) | **С** | Открытие меню вместо zero | **OK** (разделение short/long) | — |
| **A15** | CTX-04 | Divider + menu open | **С** | A/B/Select divider заморожены; joy активен | **Частично** | ALM-MENU-MOVE |
| **A16** | CTX-06 | E-Stop + вращение Mode селектора | **Н** | LCD mode ≠ `fsm_manager_get_mode` до reset | **Частично** | ALM-ESTOP-LCD |
| **A17** | CTX-00 | Смена mode на Cone/Chamfer/Divider во время jog | **Н** | **Нет** `planner_stop_all` в exit | **Частично** | ALM-MODE-NOSTOP |
| **A18** | CTX-00 | Rapid joy у soft limit | **С** | Проскок программного лимита | **OK** (by design) | — |
| **A19** | CTX-00 | MPG + Rapid у soft limit | **Н** | Проскок при rapid scale 0.1 mm | **Частично** | — |
| **A20** | CTX-00 | Retarget cruise: смена master X↔Z | **С** | `dds_motion_stop` + restart, возможен рывок | **Частично** | ALM-JOY-MASTER |
| **A21** | CTX-01 | Divider кнопки при startup | **Н** | `mode_divider_process` не вызывается | **OK** (блок) | — |
| **A22** | CTX-02 | Spdl (PPR) edit в menu | **Р** | **Не сохраняется** в `menu_save_all` | **Нет** | ALM-MENU-SPDL |
| **A23** | CTX-04 | `div_buzzer_double_short` / `limit_beep_double` | **С** | `delay()` ~125–325 ms: пропуск estop/jog poll | **Частично** | ALM-LOOP-BLOCK |
| **A24** | CTX-06 | `estop_trigger` buzzer 5×100 ms | **Р** | loop blocked ~1 s | **Частично** | ALM-LOOP-BLOCK |
| **A25** | CTX-00 | Sub EXT/INT: joy → start/pause cycle | **Н** (ожидание ТЗ) | **Нет кода**; submode не влияет | **Нет** | ALM-SUB-CYCLE |
| **A26** | CTX-05 | Вход в CYCLE (async/grind API) | **Р** | `fsm_cycle_process` не в loop — **зомби-CYCLE** | **Нет** | ALM-CYCLE-STUB |
| **A27** | CTX-00 | Несколько Lim* одновременно + Rapid click | **Р** | Берётся **первый** L→F→R→Re | **Частично** | — |
| **A28** | CTX-00 | Serial `$` команды + Joy | **Н** | Параллельный канал ввода без блокировки | **Частично** | — |

---

## 5. Сводка по FSM и подрежимам

| FSM state | Код | Jog | MPG | GO_LIM | Mode switch | Примечание |
|-----------|-----|-----|-----|--------|-------------|------------|
| IDLE | 0 | ✗ | ✗ | ✓ кнопки | ✓ | Кратковременно после init |
| MANUAL | 1 | ✓ | ✓ | ✓ | ✓ | Основной режим ручной работы |
| CYCLE | 2 | ✗ | ✗ | ✓ | ✓ | Выход CYCLE→MANUAL **отсутствует** в таблице переходов |
| PAUSED | 3 | ✗ | ✗ | ✓ | ✓ | **Мёртвое** состояние (нет вызовов pause) |
| ERROR | 4 | ✗ | ✗ | ✓ poll | ✗ FSM poll | E-Stop latch |

| Submode | Влияние на motion сегодня | По ТЗ/скилу |
|---------|--------------------------|-------------|
| MAN | Нет отдельной ветки | Базовый ручной |
| EXT | Только метка LCD | Joy → start/pause cycle |
| INT | Только метка LCD | Joy → start/pause cycle |

---

## 6. Реестр alarm-кодов (для доработки прошивки / HMI)

| Код | Условие | Severity | Recovery |
|-----|---------|----------|----------|
| ALM-MENU-MOVE | `ui_menu_is_active()` && (joy\|MPG) двигает ось | Med | Закрыть меню / добавить guard |
| ALM-GOLIM-RAPID | Rapid без feed_joy_on не стопит go_lim | Med | Отпустить rapid + joy click |
| ALM-GOLIM-BUSY | GO_LIM не стартовал из-за busy DDS | Low | Дождаться остановки / stop all |
| ALM-ESTOP-GOLIM | `go_lim_active` после estop_reset | **High** | Явный сброс go_lim в estop/motion_jog_resume |
| ALM-HW-LIM-JOG | Нет hw limit check в jog path | **High** | Аппаратный стоп / доработка hot-path |
| ALM-STARTUP-GOLIM | GO_LIM во время startup backlash | **High** | Блокировать GO_LIM при startup_busy |
| ALM-FSM-INVALID | CYCLE→MANUAL, CYCLE→IDLE | **High** | Исправить таблицу переходов FSM |
| ALM-CYCLE-JOY | Joy в CYCLE без pause | Med | Реализовать fsm_cycle_* + wiring |
| ALM-CYCLE-STUB | CYCLE без process | **High** | Не вызывать transition без process |
| ALM-JOY-MASTER | Retarget fail при смене master оси | Med | Ожидаемо; логировать |
| ALM-MODE-NOSTOP | mode exit без stop (cone/divider/chamfer) | Med | Добавить planner_stop_all |
| ALM-MENU-SPDL | Spdl не в save | Low | Добавить в menu_save_all |
| ALM-LOOP-BLOCK | delay в loop (buzzer/limits/divider) | Med | Неблокирующий бипер |
| ALM-ESTOP-LCD | Рассинхрон LCD mode и FSM при E-Stop | Low | Показывать ESTOP на LCD |
| ALM-SUB-CYCLE | Ext/Int не влияют на FSM | Med | Wiring по els-joy-feed |

---

## 7. Шаблон комбинаторной строки (для расширения)

```
ID: Sxx / Axx
Контекст: CTX-xx + FSM + mode + menu + go_lim + startup
Левая рука: {Joy:*, Rapid, Lim:*}
Правая рука: {MPG ticks, Axis, Scale}
Прочее: {Pot, Mode, Sub, Spindle, LCD, E-Stop}
Вероятность: В|С|Н|Р
Движение: {нет | X | Z | X+Z | go_lim | backlash only}
Последствия: ...
Код: файл:строки
Статус: OK | Частично | Нет | Опасно
```

---

## 8. Ключевые файлы

| Модуль | Путь |
|--------|------|
| Main loop | `01_Исходный_код/ELS_AlexA_V2.1/ELS_AlexA_V2.1.ino` |
| Кнопки / joy / limits | `src/core/ui/ui_buttons.cpp` |
| Селекторы | `src/core/ui/ui_switches.cpp` |
| Меню | `src/core/ui/ui_menu.cpp` |
| MPG ISR | `src/core/ui/ui_encoder.cpp` |
| Pot | `src/core/ui/ui_pot.cpp` |
| Jog / MPG / go_lim | `src/core/motion/motion_jog.cpp` |
| FSM | `src/core/fsm/fsm_manager.cpp`, `fsm_core.cpp` |
| Лимиты | `src/core/motion/limits.cpp` |
| Planner | `src/core/motion/planner.cpp` |
| DDS / профиль | `src/core/motion/stepper_gen.cpp` |
| E-Stop | `src/core/process/estop_control.cpp` |
| Divider | `src/modes/mode_divider/mode_divider.cpp` |
| Пины | `src/core/hal/hal_pins.h` |

---

## 9. Итог для эксплуатации

1. **Штатная двухручная работа** (joy + MPG на разных осях, pot, rapid) в `STATE_MANUAL` — **в основном OK**.
2. **Критичные дыры:** движение в меню, GO_LIM после E-Stop, невалидные переходы CYCLE, отсутствие hw-limit в jog, GO_LIM при startup.
3. **ТЗ ext/int и циклы** — в UI есть Sub, в motion **не подключено** (`fsm_cycle_process` не в loop).
4. **Divider** — единственный режим с переназначением LCD; конфликт с меню разрешён через приоритет меню.

*Версия документа: 1.0, кодовая база ELS AlexA V2.1 Stage 2.2f.*
