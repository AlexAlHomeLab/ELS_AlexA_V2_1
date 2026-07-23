/*
 * Тест цифровых пинов Arduino Mega 2560.
 * setup: все пины OUTPUT + LOW.
 * loop: по каждому пину HIGH → 100 мс → LOW.
 *
 * Внимание: не подключать к плате со включёнными драйверами/нагрузками —
 * на выходах будут короткие импульсы HIGH по всем пинам (0..69).
 */

void setup()
{
    for (uint8_t p = 0; p < NUM_DIGITAL_PINS; p++) {
        pinMode(p, OUTPUT);
        digitalWrite(p, LOW);
    }
}

void loop()
{
    for (uint8_t p = 0; p < NUM_DIGITAL_PINS; p++) {
        digitalWrite(p, HIGH);
        //delay(1);
        digitalWrite(p, LOW);
    }
}
