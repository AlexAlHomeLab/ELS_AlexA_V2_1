#include "motor_en.h"
#include "../../config/config_defs.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_pins.h"
#include <Arduino.h>

#if EN_X_SOFT_LATCH
static uint8_t en_x_latched;  /* 1 — EN_X уже включён и защёлкнут */
#endif

/* Инициализация пина EN_X: выход, мотор выкл. EN_Z не трогаем. */
void motor_en_x_init(void)
{
#if EN_X_SOFT_LATCH
    pinMode(EN_X_PIN, OUTPUT);
    EN_X_DISABLE();
    en_x_latched = 0U;
    DBG_INFO("MEN", "X", "init off");
#endif
}

/* Включить EN_X перед движением по X; повторные вызовы — no-op. */
void motor_en_x_ensure(void)
{
#if EN_X_SOFT_LATCH
    if (en_x_latched) {
        return;
    }
    EN_X_ENABLE();
    en_x_latched = 1U;
#if EN_X_SETTLE_MS > 0
    delay(EN_X_SETTLE_MS);  /* даём драйверу время на ток обмоток */
#endif
    DBG_INFO("MEN", "X", "latch on");
#endif
}

/* Снять защёлку и выключить EN_X (MODE_OFF). */
void motor_en_x_release(void)
{
#if EN_X_SOFT_LATCH
    if (!en_x_latched) {
        EN_X_DISABLE();
        return;
    }
    EN_X_DISABLE();
    en_x_latched = 0U;
    DBG_INFO("MEN", "X", "release");
#endif
}

uint8_t motor_en_x_is_latched(void)
{
#if EN_X_SOFT_LATCH
    return en_x_latched;
#else
    return 0U;
#endif
}

/* E-Stop → EN_X: пока без реализации
 * void motor_en_x_on_estop(void)
 * {
 *     motor_en_x_release();
 * }
 */
