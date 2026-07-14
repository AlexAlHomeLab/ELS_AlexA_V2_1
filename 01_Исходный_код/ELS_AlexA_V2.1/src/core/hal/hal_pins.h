#ifndef HAL_PINS_H
#define HAL_PINS_H

#include "hal_ports.h"
#include <Arduino.h>
#include <avr/io.h>

/* Пины из wokwi diagram.json — активный уровень LOW (подтяжка к +5V) */

/* --- Шаговые двигатели (этап 2) --- */
#define STEP_X_PIN 48          /* импульс STEP оси X (драйвер) */
#define DIR_X_PIN 44           /* направление DIR оси X */
#define STEP_Z_PIN 49          /* импульс STEP оси Z (драйвер) */
#define DIR_Z_PIN 43           /* направление DIR оси Z */

/* --- LCD 2004 (LiquidCrystal) --- */
#define LCD_D7_PIN 13          /* шина данных LCD, бит D7 */
#define LCD_D6_PIN 12          /* шина данных LCD, бит D6 */
#define LCD_D5_PIN 11          /* шина данных LCD, бит D5 */
#define LCD_D4_PIN 10          /* шина данных LCD, бит D4 */
#define LCD_D3_PIN 41          /* шина данных LCD, бит D3 (только 8-бит режим) */
#define LCD_D2_PIN 40          /* шина данных LCD, бит D2 (только 8-бит режим) */
#define LCD_D1_PIN 39          /* шина данных LCD, бит D1 (только 8-бит режим) */
#define LCD_D0_PIN 38          /* шина данных LCD, бит D0 (только 8-бит режим) */
#define LCD_EN_PIN 9           /* Enable LCD (строб записи/чтения) */
#define LCD_RW_PIN 23          /* R/W LCD: 0=запись, 1=чтение */
#define LCD_RS_PIN 8           /* Register Select: 0=команда, 1=данные */

/* --- Бипер (PH1 D16, активный LOW — покой HIGH, см. hal_buzzer) --- */
#define BUZZER_PIN 16          /* пьезобипер, активный LOW */

/* --- Потенциометр подачи --- */
#define POT_FEED_PIN A7        /* АЦП задатчика скорости подачи (joy/manual) */

/* --- Энкодер шпинделя D20/D21 = Port D bit1/0 --- */
#define ENC_SPINDLE_A_PIN 20   /* канал A энкодера шпинделя (INT1 / PD1) */
#define ENC_SPINDLE_B_PIN 21   /* канал B энкодера шпинделя (INT0 / PD0) */
#define ENC_SPINDLE_A_RD()  (PIND & (1 << PD1))  /* быстрое чтение A: Port D bit1 */
#define ENC_SPINDLE_B_RD()  (PIND & (1 << PD0))  /* быстрое чтение B: Port D bit0 */

/* --- РГИ D18/D19 = Port D bit3/2 --- */
#define ENC_HAND_A_PIN 18      /* канал A энкодера РГИ / MPG (INT3 / PD3) */
#define ENC_HAND_B_PIN 19      /* канал B энкодера РГИ / MPG (INT2 / PD2) */
#define ENC_HAND_A_RD()     (PIND & (1 << PD3))  /* быстрое чтение A: Port D bit3 */
#define ENC_HAND_B_RD()     (PIND & (1 << PD2))  /* быстрое чтение B: Port D bit2 */

#define HAND_AXIS_Z_PIN 2      /* селектор оси РГИ: Z (активный LOW) */
#define HAND_AXIS_X_PIN 3      /* селектор оси РГИ: X (активный LOW) */
#define HAND_AXIS_PORT_RD() (PINE & 0x30)        /* Port E bits 4–5: состояние селектора оси */

#define HAND_SCALE_X1_PIN 14   /* масштаб РГИ ×1 (активный LOW) */
#define HAND_SCALE_X10_PIN 15  /* масштаб РГИ ×10 (активный LOW) */
#define HAND_SCALE_PORT_RD() (PINJ & 0x03)       /* Port J bits 0–1: масштаб ×1/×10 */

/* --- Кнопки меню A0-A4 = Port F --- */
#define BTN_DOWN_PIN A0        /* кнопка меню «вниз» (активный LOW) */
#define BTN_UP_PIN A1          /* кнопка меню «вверх» (активный LOW) */
#define BTN_RIGHT_PIN A2       /* кнопка меню «вправо» (активный LOW) */
#define BTN_LEFT_PIN A3        /* кнопка меню «влево» (активный LOW) */
#define BTN_SELECT_PIN A4      /* кнопка меню Select / OK (активный LOW) */
#define MENU_BTN_PORT_RD()  (PINF & 0x1F)        /* Port F bits 0–4: все 5 кнопок меню */

/* --- Джойстик A8-A12 = Port K --- */
#define JOY_LEFT_PIN A8        /* джойстик: подача −Z / влево (активный LOW) */
#define JOY_RIGHT_PIN A9       /* джойстик: подача +Z / вправо (активный LOW) */
#define JOY_UP_PIN A10         /* джойстик: подача +X / вверх (активный LOW) */
#define JOY_DOWN_PIN A11       /* джойстик: подача −X / вниз (активный LOW) */
#define JOY_RAPID_PIN A12      /* джойстик: ускоренный ход Rapid (активный LOW) */
#define JOY_PORT_RD()       (PINK & 0x1F)        /* Port K bits 0–4: все 5 входов джойстика */

/* --- Подрежимы A13-A15 --- */
#define SUB_INT_PIN A13        /* подрежим Internal / INT (активный LOW) */
#define SUB_MAN_PIN A14        /* подрежим Manual / MAN (активный LOW) */
#define SUB_EXT_PIN A15        /* подрежим External / EXT (активный LOW) */
/* Селектор on-off-on: «отключённый» вход (все 3 не активны) */
#ifndef SUB_OFF_PIN
#define SUB_OFF_PIN SUB_INT_PIN /* заглушка OFF = INT (см. ui_switches) */
#endif
#define SUBMODE_PORT_RD()   (PINK & 0xE0)        /* Port K bits 5–7: INT/MAN/EXT */

/* --- Селектор режима (позиции 1–8) --- */
#define MODE_PIN_1 30          /* селектор режима, физ. пин позиции 1 (D30) */
#define MODE_PIN_2 31          /* селектор режима, физ. пин позиции 2 (D31) */
#define MODE_PIN_3 32          /* селектор режима, физ. пин позиции 3 (D32) */
#define MODE_PIN_4 33          /* селектор режима, физ. пин позиции 4 (D33) */
#define MODE_PIN_5 34          /* селектор режима, физ. пин позиции 5 (D34) */
#define MODE_PIN_6 35          /* селектор режима, физ. пин позиции 6 (D35) */
#define MODE_PIN_7 36          /* селектор режима, физ. пин позиции 7 (D36) */
#define MODE_PIN_8 37          /* селектор режима, физ. пин позиции 8 (D37); remap → config_board */

/* --- Лимиты Port A (плата 7e2: левый=D28, задний=D22) --- */
#define LIMIT_LEFT_PIN 28      /* кнопка/маркер лимита LimL (−Z), активный LOW */
#define LIMIT_FRONT_PIN 24     /* кнопка/маркер лимита LimF (+X / Front), активный LOW */
#define LIMIT_RIGHT_PIN 26     /* кнопка/маркер лимита LimR (+Z), активный LOW */
#define LIMIT_REAR_PIN 22      /* кнопка/маркер лимита LimRe (−X / Rear), активный LOW */
#define LIMIT_BTN_PORT_RD() (PINA & 0x55)        /* Port A bits 0,2,4,6: 4 кнопки лимитов */

/* --- Лимиты: LED --- */
#define LED_LIMIT_LEFT_PIN 29  /* LED лимита LimL (активный LOW) */
#define LED_LIMIT_FRONT_PIN 25 /* LED лимита LimF (активный LOW) */
#define LED_LIMIT_RIGHT_PIN 27 /* LED лимита LimR (активный LOW) */
#define LED_LIMIT_REAR_PIN 23  /* LED лимита LimRe (активный LOW); общий пин с LCD_RW */

/* --- Тахометр LED --- */
#define LED_TACHO_PIN 42       /* LED тахометра / индикатор оборотов шпинделя */

/* --- Чтение кнопок (активный LOW) --- */
#define PIN_ACTIVE_LOW(p) (hal_pin_read(p) == 0) /* true, если пин притянут к GND */
#define PIN_READ(p) PIN_ACTIVE_LOW(p)            /* алиас: чтение кнопки как активного LOW */
#define PIN_PULLUP_INPUT INPUT_PULLUP            /* режим входа с внутренней подтяжкой */

#define LIMIT_LED_ON(p)  digitalWrite(p, LOW)    /* включить LED лимита (активный LOW) */
#define LIMIT_LED_OFF(p) digitalWrite(p, HIGH)   /* выключить LED лимита */

/* --- Макросы шаговиков — прямой доступ к PORT (для ISR) --- */
#define HAL_PIN_HIGH(p) (*portOutputRegister(digitalPinToPort(p)) |= digitalPinToBitMask(p))
#define HAL_PIN_LOW(p)  (*portOutputRegister(digitalPinToPort(p)) &= ~digitalPinToBitMask(p))
#define DIR_X_SET(d)  ((d) ? HAL_PIN_HIGH(DIR_X_PIN) : HAL_PIN_LOW(DIR_X_PIN)) /* DIR X: 1=HIGH */
#define DIR_Z_SET(d)  ((d) ? HAL_PIN_HIGH(DIR_Z_PIN) : HAL_PIN_LOW(DIR_Z_PIN)) /* DIR Z: 1=HIGH */
#define STEP_X_ON()   HAL_PIN_HIGH(STEP_X_PIN)   /* фронт STEP X */
#define STEP_X_OFF()  HAL_PIN_LOW(STEP_X_PIN)    /* спад STEP X */
#define STEP_Z_ON()   HAL_PIN_HIGH(STEP_Z_PIN)   /* фронт STEP Z */
#define STEP_Z_OFF()  HAL_PIN_LOW(STEP_Z_PIN)    /* спад STEP Z */

#define LIMIT_UP_PIN    LIMIT_FRONT_PIN          /* алиас: лимит «вверх» по X = Front */
#define LIMIT_DOWN_PIN  LIMIT_REAR_PIN           /* алиас: лимит «вниз» по X = Rear */

#define ESTOP_BTN_PIN     47 /* авария E-stop (активный LOW); было 40 — занят LCD_D2 в 8-бит */
#define SPINDLE_PWM_PIN   6  /* ШИМ оборотов шпинделя (OC4A / Timer4) */
#define COOLANT_PIN       17 /* реле/выход СОЖ (охлаждение) */

/* --- Port D: энкодеры шпинделя и РГИ (как 7e2 Encoder_Init) --- */
#define ENC_PORTD_INIT() do { \
    DDRD &= ~((1 << PD0) | (1 << PD1) | (1 << PD2) | (1 << PD3)); \
    PORTD |= ((1 << PD0) | (1 << PD1) | (1 << PD2) | (1 << PD3)); \
} while (0)  /* входы PD0–3 + подтяжка: шпиндель A/B и РГИ A/B */

#endif
