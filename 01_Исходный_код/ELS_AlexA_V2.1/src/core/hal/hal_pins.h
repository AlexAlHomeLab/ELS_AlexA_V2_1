#ifndef HAL_PINS_H
#define HAL_PINS_H

#include <Arduino.h>

/* Пины из wokwi diagram.json — активный уровень LOW (подтяжка к +5V) */

/* --- Шаговые двигатели (этап 2) --- */
#define STEP_X_PIN 48
#define DIR_X_PIN 44
#define STEP_Z_PIN 49
#define DIR_Z_PIN 43

/* --- LCD 2004 (LiquidCrystal) --- */
#define LCD_RS_PIN 8
#define LCD_EN_PIN 9
#define LCD_D4_PIN 10
#define LCD_D5_PIN 11
#define LCD_D6_PIN 12
#define LCD_D7_PIN 13

/* --- Бипер --- */
#define BUZZER_PIN 16

/* --- Потенциометр подачи --- */
#define POT_FEED_PIN A7

/* --- Энкодер шпинделя --- */
#define ENC_SPINDLE_A_PIN 20
#define ENC_SPINDLE_B_PIN 21

/* --- РГИ (ручной генератор) --- */
#define ENC_HAND_A_PIN 18
#define ENC_HAND_B_PIN 19
#define HAND_AXIS_Z_PIN 2
#define HAND_AXIS_X_PIN 3
#define HAND_SCALE_X1_PIN 15
#define HAND_SCALE_X10_PIN 14

/* --- Кнопки меню под дисплеем --- */
#define BTN_DOWN_PIN A0
#define BTN_UP_PIN A1
#define BTN_RIGHT_PIN A2
#define BTN_LEFT_PIN A3
#define BTN_SELECT_PIN A4

/* --- Джойстик --- */
#define JOY_LEFT_PIN A8
#define JOY_RIGHT_PIN A9
#define JOY_UP_PIN A10
#define JOY_DOWN_PIN A11
#define JOY_RAPID_PIN A12

/* --- Подрежимы man / ext / int --- */
#define SUB_INT_PIN A13
#define SUB_MAN_PIN A14
#define SUB_EXT_PIN A15

/* --- Селектор режима (позиции 1–8) --- */
#define MODE_PIN_1 30
#define MODE_PIN_2 31
#define MODE_PIN_3 32
#define MODE_PIN_4 33
#define MODE_PIN_5 34
#define MODE_PIN_6 35
#define MODE_PIN_7 36
#define MODE_PIN_8 37

/* --- Лимиты: входы --- */
#define LIMIT_LEFT_PIN 22
#define LIMIT_FRONT_PIN 24
#define LIMIT_RIGHT_PIN 26
#define LIMIT_REAR_PIN 28

/* --- Лимиты: LED --- */
#define LED_LIMIT_LEFT_PIN 29
#define LED_LIMIT_FRONT_PIN 25
#define LED_LIMIT_RIGHT_PIN 27
#define LED_LIMIT_REAR_PIN 23

/* --- Тахометр LED --- */
#define LED_TACHO_PIN 42

/* --- Чтение кнопок (активный LOW) --- */
#define PIN_ACTIVE_LOW(p) (digitalRead(p) == LOW)
#define PIN_READ(p) PIN_ACTIVE_LOW(p)
#define PIN_PULLUP_INPUT INPUT_PULLUP

/* LED лимитов: катод на пин, анод на +5V через резистор → активный LOW */
#define LIMIT_LED_ON(p)  digitalWrite(p, LOW)
#define LIMIT_LED_OFF(p) digitalWrite(p, HIGH)

/* --- Макросы шаговиков (заглушки, этап 2) --- */
#define DIR_X_SET(d)  digitalWrite(DIR_X_PIN, (d) ? HIGH : LOW)
#define DIR_Z_SET(d)  digitalWrite(DIR_Z_PIN, (d) ? HIGH : LOW)
#define STEP_X_ON()   digitalWrite(STEP_X_PIN, HIGH)
#define STEP_X_OFF()  digitalWrite(STEP_X_PIN, LOW)
#define STEP_Z_ON()   digitalWrite(STEP_Z_PIN, HIGH)
#define STEP_Z_OFF()  digitalWrite(STEP_Z_PIN, LOW)

/* Алиасы имён из KubitDoc-заглушек */
#define LIMIT_UP_PIN    LIMIT_FRONT_PIN
#define LIMIT_DOWN_PIN  LIMIT_REAR_PIN

#define ESTOP_BTN_PIN     BTN_SELECT_PIN
#define SPINDLE_PWM_PIN   6
#define COOLANT_PIN       17

#endif
