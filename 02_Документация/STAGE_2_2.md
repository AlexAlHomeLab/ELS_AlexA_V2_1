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

## 2.2f — API шпинделя для синхронизации
- `spindle_get_count`, `spindle_get_delta`, `spindle_get_dir`
- Подготовка к этапу 3 (Thread/Sync)
