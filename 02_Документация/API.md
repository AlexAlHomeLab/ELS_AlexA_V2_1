API ELS AlexA V2.1

Основные функции

Управление движением

motion_init()
Инициализация системы управления движением.

motion_move_abs(x, z, speed)
Абсолютное перемещение в координаты (x, z) со скоростью speed (мм/мин).

motion_move_rel(dx, dz, speed)
Относительное перемещение на (dx, dz) со скоростью speed (мм/мин).

motion_stop()
Экстренная остановка всех движений.

motion_is_moving()
Проверка: выполняется ли движение.

Планировщик

planner_add_move(x, z, speed)
Добавление движения в планировщик.

planner_stop_all()
Остановка всех запланированных движений.

planner_is_empty()
Проверка: есть ли блоки в планировщике.

DDS генератор шагов

dds_init()
Инициализация DDS генератора.

dds_set_speed(axis, steps_per_sec)
Установка скорости для оси (шагов/сек).

dds_set_direction(axis, dir)
Установка направления для оси.

dds_enable(axis, enable)
Включение/отключение оси.

dds_get_position(axis)
Получение текущей позиции оси (импульсы).

dds_set_position(axis, pos)
Установка позиции оси (импульсы).

dds_set_target(axis, target)
Установка целевой позиции оси.

Управление шпинделем

spindle_set_speed(pwm)
Установка скорости шпинделя (PWM 0-255).

spindle_get_rpm()
Получение текущих оборотов шпинделя.

spindle_get_frequency()
Получение частоты энкодера шпинделя (Гц).

Управление СОЖ

coolant_on()
Включение СОЖ.

coolant_off()
Выключение СОЖ.

coolant_toggle()
Переключение состояния СОЖ.

E-Stop

estop_check()
Проверка состояния E-Stop.

estop_trigger()
Активация аварийной остановки.

estop_reset()
Сброс E-Stop.

FSM

fsm_transition(new_state)
Переход в новое состояние.

fsm_get_state()
Получение текущего состояния.

fsm_error(error_code)
Обработка ошибки.

Режимы

mode_async_enter()
Вход в режим асинхронной подачи.

mode_sync_enter()
Вход в режим синхронной подачи.

mode_thread_enter()
Вход в режим нарезания резьбы.

mode_chamfer_enter()
Вход в режим фаски.

mode_cone_enter()
Вход в режим конуса.

mode_sphere_enter()
Вход в режим сферы.

mode_divider_enter()
Вход в режим делительной головки.

mode_grind_enter()
Вход в режим шлифовки.