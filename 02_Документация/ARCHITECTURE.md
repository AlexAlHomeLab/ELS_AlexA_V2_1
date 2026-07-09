Архитектура ELS AlexA V2.1

Общая архитектура

Система построена по модульному принципу с разделением на слои.

Слои архитектуры (сверху вниз):

1. UI Layer (ui_*) - интерфейс пользователя
2. FSM Layer (fsm_*) - конечные автоматы
3. Modes Layer (mode_*) - режимы работы
4. Motion Core (planner, stepper) - планировщик и генерация шагов
5. Process Control (spindle, coolant) - управление процессами
6. HAL Layer (hal_*) - аппаратная абстракция
7. Hardware - железо

Описание слоёв

1. HAL (Hardware Abstraction Layer)
- Вся работа с железом только здесь
- Прямые регистры только в HAL
- Унифицированный API для всех платформ

Модули HAL:
- hal_pins.h - определение пинов
- hal_timers.c - настройка таймеров
- hal_interrupts.c - внешние прерывания
- hal_ports.c - быстрые операции с портами

2. Motion Core
- Планировщик движений (на основе GRBL)
- Генерация шагов (DDS)
- Компенсация люфта
- Лимиты и защита

Модули Motion:
- planner.c - планировщик с look-ahead
- stepper_gen.c - DDS генератор шагов
- backlash.c - компенсация люфта
- limits.c - программные лимиты

3. Process Control
- Управление шпинделем
- Управление СОЖ
- E-Stop

Модули Process:
- spindle_control.c - шпиндель
- coolant_control.c - СОЖ
- estop_control.c - аварийная остановка

4. UI Layer
- Работа с дисплеем (hd44780)
- Обработка кнопок (EncButton)
- Энкодеры и потенциометр
- Меню и отображение

Модули UI:
- ui_lcd.c - дисплей
- ui_buttons.c - кнопки
- ui_encoder.c - энкодеры
- ui_pot.c - потенциометр
- ui_menu.c - меню

5. FSM (Finite State Machine)
- Управление состояниями системы
- Обработка переходов
- Циклы ext/int

Модули FSM:
- fsm_core.c - базовый конечный автомат
- fsm_manager.c - менеджер состояний
- fsm_cycles.c - циклы обработки

6. Modes (Режимы)
- Каждый режим в отдельной папке
- Единый API для всех режимов
- Плагинная архитектура

Режимы:
0 - Асинхронная подача (mode_async)
1 - Синхронная подача (mode_sync)
2 - Фаска (mode_chamfer)
3 - Нарезание резьбы (mode_thread)
4 - Конус левый (mode_cone)
5 - Конус правый (mode_cone)
6 - Делительная головка (mode_divider)
7 - Сфера (mode_sphere)
8 - Шлифовка (mode_grind)

7. Debug
- Телеметрия в структурированном формате
- Условная компиляция
- Отключаемая отладка

Модули Debug:
- debug_telemetry.c - телеметрия
- debug_serial.c - отладка через Serial

Зависимости между слоями:
- HAL - не зависит от других слоёв
- Motion - зависит от HAL
- Process - зависит от HAL
- UI - зависит от HAL
- FSM - зависит от Motion, Process, UI
- Modes - зависит от FSM, Motion

Принципы проектирования:
1. Single Responsibility - каждый модуль делает одну вещь
2. Open/Closed - открыт для расширения, закрыт для изменения
3. Loose Coupling - минимум зависимостей
4. High Cohesion - внутри модуля всё логически связано
5. Dependency Inversion - зависимости от абстракций