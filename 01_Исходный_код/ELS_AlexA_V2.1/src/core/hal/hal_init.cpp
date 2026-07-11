#include "hal_init.h"
#include "hal_buzzer.h"
#include "hal_pins.h"

static void hal_pin_input(uint8_t pin) {
    pinMode(pin, INPUT_PULLUP);
}

static void hal_pin_output_low(uint8_t pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

static void hal_pin_output_high(uint8_t pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
}

void hal_init(void) {
    /* Моторы — выходы, покой LOW (этап 2) */
    hal_pin_output_low(STEP_X_PIN);
    hal_pin_output_low(DIR_X_PIN);
    hal_pin_output_low(STEP_Z_PIN);
    hal_pin_output_low(DIR_Z_PIN);

    hal_buzzer_init();
    hal_pin_output_high(LED_TACHO_PIN);
    hal_pin_output_high(LED_LIMIT_LEFT_PIN);
    hal_pin_output_high(LED_LIMIT_FRONT_PIN);
    hal_pin_output_high(LED_LIMIT_RIGHT_PIN);
    hal_pin_output_high(LED_LIMIT_REAR_PIN);

    /* Кнопки, джойстик, подрежимы */
    hal_pin_input(BTN_DOWN_PIN);
    hal_pin_input(BTN_UP_PIN);
    hal_pin_input(BTN_RIGHT_PIN);
    hal_pin_input(BTN_LEFT_PIN);
    hal_pin_input(BTN_SELECT_PIN);
    hal_pin_input(JOY_LEFT_PIN);
    hal_pin_input(JOY_RIGHT_PIN);
    hal_pin_input(JOY_UP_PIN);
    hal_pin_input(JOY_DOWN_PIN);
    hal_pin_input(JOY_RAPID_PIN);
    hal_pin_input(SUB_INT_PIN);
    hal_pin_input(SUB_MAN_PIN);
    hal_pin_input(SUB_EXT_PIN);

    /* Селектор режима */
    hal_pin_input(MODE_PIN_1);
    hal_pin_input(MODE_PIN_2);
    hal_pin_input(MODE_PIN_3);
    hal_pin_input(MODE_PIN_4);
    hal_pin_input(MODE_PIN_5);
    hal_pin_input(MODE_PIN_6);
    hal_pin_input(MODE_PIN_7);
    hal_pin_input(MODE_PIN_8);

    /* РГИ / оси */
    hal_pin_input(ENC_HAND_A_PIN);
    hal_pin_input(ENC_HAND_B_PIN);
    hal_pin_input(HAND_AXIS_Z_PIN);
    hal_pin_input(HAND_AXIS_X_PIN);
    hal_pin_input(HAND_SCALE_X1_PIN);
    hal_pin_input(HAND_SCALE_X10_PIN);

    /* Энкодер шпинделя */
    hal_pin_input(ENC_SPINDLE_A_PIN);
    hal_pin_input(ENC_SPINDLE_B_PIN);

    /* Лимиты */
    hal_pin_input(LIMIT_LEFT_PIN);
    hal_pin_input(LIMIT_FRONT_PIN);
    hal_pin_input(LIMIT_RIGHT_PIN);
    hal_pin_input(LIMIT_REAR_PIN);
    hal_pin_input(ESTOP_BTN_PIN);
}
