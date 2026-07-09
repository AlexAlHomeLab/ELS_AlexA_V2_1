#ifndef HAL_PINS_H
#define HAL_PINS_H

#include "hal_ports.h"
#include <Arduino.h>
#include <avr/io.h>

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

/* --- Энкодер шпинделя D20/D21 = Port D bit1/0 --- */
#define ENC_SPINDLE_A_PIN 20
#define ENC_SPINDLE_B_PIN 21
#define ENC_SPINDLE_A_RD()  (PIND & (1 << PD1))
#define ENC_SPINDLE_B_RD()  (PIND & (1 << PD0))

/* --- РГИ D18/D19 = Port D bit3/2 --- */
#define ENC_HAND_A_PIN 18
#define ENC_HAND_B_PIN 19
#define ENC_HAND_A_RD()     (PIND & (1 << PD3))
#define ENC_HAND_B_RD()     (PIND & (1 << PD2))

#define HAND_AXIS_Z_PIN 2
#define HAND_AXIS_X_PIN 3
#define HAND_AXIS_PORT_RD() (PINE & 0x30)

#define HAND_SCALE_X1_PIN 15
#define HAND_SCALE_X10_PIN 14
#define HAND_SCALE_PORT_RD() (PINJ & 0x03)

/* --- Кнопки меню A0-A4 = Port F --- */
#define BTN_DOWN_PIN A0
#define BTN_UP_PIN A1
#define BTN_RIGHT_PIN A2
#define BTN_LEFT_PIN A3
#define BTN_SELECT_PIN A4
#define MENU_BTN_PORT_RD()  (PINF & 0x1F)

/* --- Джойстик A8-A12 = Port K --- */
#define JOY_LEFT_PIN A8
#define JOY_RIGHT_PIN A9
#define JOY_UP_PIN A10
#define JOY_DOWN_PIN A11
#define JOY_RAPID_PIN A12
#define JOY_PORT_RD()       (PINK & 0x1F)

/* --- Подрежимы A13-A15 --- */
#define SUB_INT_PIN A13
#define SUB_MAN_PIN A14
#define SUB_EXT_PIN A15
/* Селектор on-off-on: «отключённый» вход (все 3 не активны) */
#ifndef SUB_OFF_PIN
#define SUB_OFF_PIN SUB_MAN_PIN
#endif
#define SUBMODE_PORT_RD()   (PINK & 0xE0)

/* --- Селектор режима (позиции 1–8) --- */
#define MODE_PIN_1 30
#define MODE_PIN_2 31
#define MODE_PIN_3 32
#define MODE_PIN_4 33
#define MODE_PIN_5 34
#define MODE_PIN_6 35
#define MODE_PIN_7 36
#define MODE_PIN_8 37

/* --- Лимиты Port A --- */
#define LIMIT_LEFT_PIN 22
#define LIMIT_FRONT_PIN 24
#define LIMIT_RIGHT_PIN 26
#define LIMIT_REAR_PIN 28
#define LIMIT_BTN_PORT_RD() (PINA & 0x55)

/* --- Лимиты: LED --- */
#define LED_LIMIT_LEFT_PIN 29
#define LED_LIMIT_FRONT_PIN 25
#define LED_LIMIT_RIGHT_PIN 27
#define LED_LIMIT_REAR_PIN 23

/* --- Тахометр LED --- */
#define LED_TACHO_PIN 42

/* --- Чтение кнопок (активный LOW) --- */
#define PIN_ACTIVE_LOW(p) (hal_pin_read(p) == 0)
#define PIN_READ(p) PIN_ACTIVE_LOW(p)
#define PIN_PULLUP_INPUT INPUT_PULLUP

#define LIMIT_LED_ON(p)  digitalWrite(p, LOW)
#define LIMIT_LED_OFF(p) digitalWrite(p, HIGH)

/* --- Макросы шаговиков — прямой доступ к PORT (для ISR) --- */
#define HAL_PIN_HIGH(p) (*portOutputRegister(digitalPinToPort(p)) |= digitalPinToBitMask(p))
#define HAL_PIN_LOW(p)  (*portOutputRegister(digitalPinToPort(p)) &= ~digitalPinToBitMask(p))
#define DIR_X_SET(d)  ((d) ? HAL_PIN_HIGH(DIR_X_PIN) : HAL_PIN_LOW(DIR_X_PIN))
#define DIR_Z_SET(d)  ((d) ? HAL_PIN_HIGH(DIR_Z_PIN) : HAL_PIN_LOW(DIR_Z_PIN))
#define STEP_X_ON()   HAL_PIN_HIGH(STEP_X_PIN)
#define STEP_X_OFF()  HAL_PIN_LOW(STEP_X_PIN)
#define STEP_Z_ON()   HAL_PIN_HIGH(STEP_Z_PIN)
#define STEP_Z_OFF()  HAL_PIN_LOW(STEP_Z_PIN)

#define LIMIT_UP_PIN    LIMIT_FRONT_PIN
#define LIMIT_DOWN_PIN  LIMIT_REAR_PIN

#define ESTOP_BTN_PIN     BTN_SELECT_PIN
#define SPINDLE_PWM_PIN   6
#define COOLANT_PIN       17

/* --- Port D: энкодеры шпинделя и РГИ (как 7e2 Encoder_Init) --- */
#define ENC_PORTD_INIT() do { \
    DDRD &= ~((1 << PD0) | (1 << PD1) | (1 << PD2) | (1 << PD3)); \
    PORTD |= ((1 << PD0) | (1 << PD1) | (1 << PD2) | (1 << PD3)); \
} while (0)

#endif
