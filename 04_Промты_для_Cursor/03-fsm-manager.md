# Промт: FSM Manager


Реализовать управление состояниями системы.


1. Конечные автоматы для всех режимов
2. Обработка переходов между состояниями
3. Циклы ext/int
4. Обработка ошибок




- Базовые состояния (Idle, Running, Paused, Error)
- Обработчики переходов
- Валидация переходов


- Управление текущим режимом
- Переключение подрежимов (man/ext/int)
- Координация между FSM


- Циклы внешнего точения (ext)
- Циклы внутреннего точения (int)
- Обработка пауз и продолжений


- STATE_IDLE - ожидание
- STATE_MANUAL - ручной режим
- STATE_CYCLE - выполнение цикла
- STATE_PAUSED - пауза
- STATE_ERROR - ошибка


IDLE → MANUAL (выбор режима)
MANUAL → CYCLE (запуск джойстиком)
CYCLE → PAUSED (остановка джойстика)
PAUSED → CYCLE (продолжение)
ANY → ERROR (ошибка)
ERROR → IDLE (сброс)


void fsm_transition(State_t new_state) {
if (fsm_validate_transition(current_state, new_state)) {
fsm_exit(current_state);
current_state = new_state;
fsm_enter(current_state);
}
}


Реализуйте FSM для управления режимами и циклами. Обеспечьте корректные переходы и обработку ошибок.