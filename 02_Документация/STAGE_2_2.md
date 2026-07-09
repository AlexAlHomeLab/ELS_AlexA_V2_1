# Этап 2.2 — подэтапы

## 2.2a — Единый API координат (motion_control) ✅
- `motion_get_pos_steps/mm`, `motion_set_pos_steps`, `motion_zero_all`
- LCD и motion_jog через motion_control

## 2.2b — FSM в главном цикле ✅
- `fsm_manager_init/poll/process` в loop
- Ручной jog только в STATE_MANUAL
- E-Stop → STATE_ERROR, `estop_reset` → STATE_MANUAL
- Селектор режимов 1..8 → индекс 0..7 (Async..Grind)

## 2.2c — Planner → DDS ✅
- `planner_process()` — цель в мм → шаги DDS, пропорциональные скорости осей
- `planner_is_busy()` — блокирует jog при выполнении очереди
- `motion_move_abs/rel` через planner

## 2.2d — Программируемые лимиты ✅
- Кнопки LimL/LimR → Z, LimF/LimRe → X: запись текущей координаты, LED, повтор — сброс
- `limits_clamp_steps()` в jog и planner

## 2.2e — Накопитель РГИ ✅
- `hand_pos` сбрасывается **только** при начале движения джойстиком по этой оси
- Смена масштаба и оси РГИ накопитель **не** сбрасывает
- Координаты при сбросе накопителя не меняются

## 2.2f — API шпинделя и потенциометр по режимам ✅
- `spindle_get_count`, `spindle_get_delta`, `spindle_get_dir`
- Потенциометр: фильтр 16×, инверсия как в 7e2
- **2 диапазона в EEPROM:** async mm/min (20–400), sync mm/rev (0.02–0.20)
- **Меню:** aMin/aMax, sMin/sMax; Z/X: stp, uSt, pit, **max, rap, acc**; Spdl, Buzzer
- `config_machine`: параметры **каждой оси отдельно** (мотор, винт, max/rapid mm/min, accel)
- **Serial CLI (GRBL $):** `$$`, `$n`, `$n=val`, `$I`, `?` — см. DEBUG.md
- Async mm/min: Async, Chamfer, Cone, Sphere, Divider, Grind
- Sync mm/rev: только Sync
- **Thread:** шаг резьбы из меню/таблицы, пот не используется (`F:---`), jog — async mm/min
