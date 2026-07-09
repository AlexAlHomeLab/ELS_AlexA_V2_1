Аппаратное обеспечение ELS AlexA V2.1

Платформа

- Контроллер: Arduino Mega 2560
- Тактовая частота: 16 МГц
- Flash память: 256 КБ
- SRAM: 8 КБ
- EEPROM: 4 КБ

Подключение пинов

Шаговые двигатели:
STEP_X - пин 2
DIR_X - пин 3
EN_X - пин 4
STEP_Z - пин 5
DIR_Z - пин 6
EN_Z - пин 7

Энкодер шпинделя:
ENCODER_A - пин 18 (INT3)
ENCODER_B - пин 19 (INT4)

РГИ (ручной генератор импульсов):
MPG_A - пин 20 (INT5)
MPG_B - пин 21 (INT6)

Дисплей LCD 2004:
RS - пин 8
EN - пин 9
D4 - пин 4
D5 - пин 5
D6 - пин 6
D7 - пин 7

Кнопки под дисплеем:
LEFT - пин 22
RIGHT - пин 23
UP - пин 24
DOWN - пин 25
SELECT - пин 26

Селекторы:
MODE (0-9) - пин A0
SUBMODE (man/ext/int) - пин A1
AXIS (X/Z) - пин A2
RESOLUTION (1:1/0.01mm) - пин A3

Джойстик подач:
LEFT - пин 27
RIGHT - пин 28
UP - пин 29
DOWN - пин 30

Лимиты с подсветкой:
LIMIT_LEFT - пин 31
LIMIT_RIGHT - пин 32
LIMIT_UP - пин 33
LIMIT_DOWN - пин 34
LED_LEFT - пин 35
LED_RIGHT - пин 36
LED_UP - пин 37
LED_DOWN - пин 38

Управление:
RAPID - пин 39
ESTOP - пин 40
ENABLE - пин 41

Процесс:
SPINDLE_PWM - пин 42
SPINDLE_DIR - пин 43
COOLANT - пин 44
BUZZER - пин 45

Схема подключения доступна в папке 06_Схемы_и_чертежи