/*
 * ELS AlexA V2.1 — этап 2.1: MPG jog + DDS
 * Arduino Mega 2560, пины из wokwi diagram.json
 */

#include "src/config/config.h"
#include "src/config/config_storage.h"
#include "src/core/debug/debug_serial.h"
#include "src/core/hal/hal_init.h"
#include "src/core/hal/hal_interrupts.h"
#include "src/core/hal/hal_timers.h"
#include "src/core/motion/limits.h"
#include "src/core/motion/motion_control.h"
#include "src/core/motion/motion_jog.h"
#include "src/core/motion/stepper_gen.h"
#include "src/core/process/estop_control.h"
#include "src/core/process/spindle_control.h"
#include "src/core/ui/ui_buttons.h"
#include "src/core/ui/ui_encoder.h"
#include "src/core/ui/ui_lcd.h"
#include "src/core/ui/ui_menu.h"
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

static int32_t lcd_axis_coord(uint8_t axis) {
    return motion_jog_get_pos(axis);
}

static void update_main_lcd(void) {
    SwitchState_t sw = ui_switches_get_state();
    uint8_t feed_pct = ui_pot_get_percent();
    uint8_t feed_max = config_get_feed_max();
    uint8_t feed_show = (uint8_t)((feed_pct * feed_max) / 100U);

    snprintf(lcd_line, sizeof(lcd_line), "M:%u %s Mx:%u",
             sw.mode, submode_name(sw.submode), config_get_feed_max());
    ui_lcd_set_line(1, lcd_line);

    char axis = (sw.mpg_axis == AXIS_X) ? 'X' : 'Z';
    snprintf(lcd_line, sizeof(lcd_line), "MPG %c %+ld F:%u%%",
             axis, (long)motion_jog_get_hand(sw.mpg_axis), feed_show);
    ui_lcd_set_line(2, lcd_line);

    snprintf(lcd_line, sizeof(lcd_line), "X:%+ld Z:%+ld RPM:%lu",
             (long)lcd_axis_coord(AXIS_X),
             (long)lcd_axis_coord(AXIS_Z),
             (unsigned long)spindle_get_rpm());
    ui_lcd_set_line(3, lcd_line);
}

static void update_lcd(void) {
    ui_lcd_set_line(0, FIRMWARE_NAME);
    if (ui_menu_is_active()) {
        ui_menu_lcd();
    } else {
        update_main_lcd();
    }
}

void setup() {
    hal_init();
    debug_serial_init(SERIAL_BAUD);

    debug_serial_println("");
    debug_serial_println(FIRMWARE_NAME " " FIRMWARE_STAGE " Starting...");

    config_load();
    motion_init();
    motion_jog_init();
    timer1_init(STEP_ISR_PERIOD_US);

    ui_lcd_init();
    ui_switches_init();
    ui_buttons_init();
    limits_ui_init();
    ui_pot_init();
    ui_encoder_init();
    ui_menu_init();
    spindle_init();
    encoder_interrupts_init();

    update_lcd();
    ui_lcd_update();

    DBG_INFO("SYS", "INIT", "Stage 2 jog ready");
}

void loop() {
    estop_check();
    ui_buttons_poll();
    ui_switches_poll();
    ui_encoder_poll();
    motion_jog_joy_poll();
    motion_jog_poll();
    ui_menu_poll();
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
