/*
 * ELS AlexA V2.1 — этап 1 (шаги 1–9)
 * Arduino Mega 2560, пины из wokwi diagram.json
 */

#include "src/config/config.h"
#include "src/core/debug/debug_serial.h"
#include "src/core/hal/hal_init.h"
#include "src/core/motion/limits.h"
#include "src/core/process/spindle_control.h"
#include "src/core/ui/ui_buttons.h"
#include "src/core/ui/ui_encoder.h"
#include "src/core/ui/ui_lcd.h"
#include "src/core/ui/ui_pot.h"
#include "src/core/ui/ui_switches.h"

#include <Arduino.h>

static unsigned long last_lcd_ms = 0;
static unsigned long last_pot_ms = 0;
static char lcd_line[LCD_COLS + 1];

static const char *submode_name(uint8_t sub) {
    if (sub == SUB_EXT) return "Ext";
    if (sub == SUB_INT) return "Int";
    return "Man";
}

static void update_lcd(void) {
    SwitchState_t sw = ui_switches_get_state();

    snprintf(lcd_line, sizeof(lcd_line), "M:%u %s",
             sw.mode, submode_name(sw.submode));
    ui_lcd_set_line(1, lcd_line);

    char axis = (sw.mpg_axis == AXIS_X) ? 'X' : 'Z';
    snprintf(lcd_line, sizeof(lcd_line), "MPG %c %+ld F:%u%%",
             axis, (long)ui_encoder_get_mpg_pos(), ui_pot_get_percent());
    ui_lcd_set_line(2, lcd_line);

    snprintf(lcd_line, sizeof(lcd_line), "L%c F%c R%c Re%c RPM:%lu",
             limits_ui_led_on(0) ? '*' : ' ',
             limits_ui_led_on(1) ? '*' : ' ',
             limits_ui_led_on(2) ? '*' : ' ',
             limits_ui_led_on(3) ? '*' : ' ',
             (unsigned long)spindle_get_rpm());
    ui_lcd_set_line(3, lcd_line);
}

void setup() {
    hal_init();
    debug_serial_init(SERIAL_BAUD);

    debug_serial_println("");
    debug_serial_println(FIRMWARE_NAME " " FIRMWARE_STAGE " Starting...");

    ui_lcd_init();
    ui_switches_init();
    ui_buttons_init();
    limits_ui_init();
    ui_pot_init();
    ui_encoder_init();
    spindle_init();

    ui_lcd_set_line(0, FIRMWARE_NAME);
    ui_lcd_set_line(1, FIRMWARE_STAGE " Ready");
    update_lcd();
    ui_lcd_update();

    DBG_INFO("SYS", "INIT", "Stage 1 ready");
}

void loop() {
    ui_buttons_poll();
    ui_switches_poll();
    ui_encoder_poll();
    spindle_poll();

    if (millis() - last_pot_ms >= POT_READ_MS) {
        if (ui_pot_poll()) {
            DBG_INFO_VAL("UI", "POT", "pct", ui_pot_get_percent());
        }
        last_pot_ms = millis();
    }

    update_lcd();

    if (millis() - last_lcd_ms >= LCD_UPDATE_MS) {
        ui_lcd_update();
        last_lcd_ms = millis();
    }
}
