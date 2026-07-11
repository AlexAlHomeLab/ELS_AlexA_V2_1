#include "hal_buzzer.h"
#include "hal_pins.h"
#include "../../config/config_storage.h"
#include <Arduino.h>

/* Активный LOW на D16 (PH1), как Beeper_On/Off в Digital_Feed_7e2. */

static void buzzer_on(void) {
    digitalWrite(BUZZER_PIN, LOW);
}

static void buzzer_off(void) {
    digitalWrite(BUZZER_PIN, HIGH);
}

void hal_buzzer_init(void) {
    pinMode(BUZZER_PIN, OUTPUT);
    buzzer_off();
}

void hal_buzzer_beep_ms(uint16_t ms) {
    if (!config_get_buzzer_on()) {
        return;
    }
    buzzer_on();
    delay(ms);
    buzzer_off();
}

void hal_buzzer_beep_double_ms(uint16_t ms, uint16_t gap_ms) {
    if (!config_get_buzzer_on()) {
        return;
    }
    hal_buzzer_beep_ms(ms);
    delay(gap_ms);
    hal_buzzer_beep_ms(ms);
}

void hal_buzzer_estop_signal(void) {
    for (int i = 0; i < 5; i++) {
        buzzer_on();
        delay(100);
        buzzer_off();
        delay(100);
    }
}
