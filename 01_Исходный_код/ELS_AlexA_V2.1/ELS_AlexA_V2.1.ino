/*
 * ELS AlexA V2.1 — этап 2.2f: API шпинделя, потенциометр по режимам
 * Arduino Mega 2560, пины из wokwi diagram.json
 */

#include "src/config/config.h"
#include "src/config/config_storage.h"
#include "src/core/debug/debug_serial.h"
#include "src/core/debug/serial_config.h"
#include "src/core/hal/hal_init.h"
#include "src/core/hal/hal_interrupts.h"
#include "src/core/hal/hal_timers.h"
#include "src/core/fsm/fsm_manager.h"
#include "src/core/motion/limits.h"
#include "src/core/motion/backlash.h"
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
#include <stdio.h>
#include <string.h>

static unsigned long last_lcd_ms = 0;
static unsigned long last_pot_ms = 0;
static uint8_t startup_armed = 0;
static char lcd_line[LCD_COLS + 1];

static const char *submode_short(uint8_t sub) {
    if (sub == SUB_EXT) return "Ext";
    if (sub == SUB_INT) return "Int";
    return "Man";
}

static void lcd_format_status_line(const char *mode, const char *feed, const char *sub,
                                   char *buf, size_t len) {
    (void)len;
    memset(buf, ' ', LCD_COLS);

    size_t fl = strlen(feed);
    if (fl > LCD_COLS) fl = LCD_COLS;
    uint8_t fp = (uint8_t)((LCD_COLS - fl) / 2U);
    memcpy(buf + fp, feed, fl);

    size_t ml = strlen(mode);
    uint8_t max_ml = fp > 0U ? fp : 1U;
    if (ml > max_ml) ml = max_ml;
    memcpy(buf, mode, ml);

    size_t sl = strlen(sub);
    if (sl > 3U) sl = 3U;
    memcpy(buf + LCD_COLS - 3U, sub, sl);

    buf[LCD_COLS] = 0;
}

static int lcd_format_coord(char *dst, size_t len, char axis, long val) {
    if (val < 0) {
        return snprintf(dst, len, "%c-%07ld", axis, -val);
    }
    return snprintf(dst, len, "%c %07ld", axis, val);
}

static void lcd_format_coords_line(char *buf, size_t len) {
    char xp[10];
    char zp[10];
    lcd_format_coord(xp, sizeof(xp), 'X', (long)motion_get_pos_steps(AXIS_X));
    lcd_format_coord(zp, sizeof(zp), 'Z', (long)motion_get_pos_steps(AXIS_Z));
    snprintf(buf, len, "%s %s", xp, zp);
}

static void update_main_lcd(void) {
    if (backlash_startup_busy()) {
        ui_lcd_set_line(0, "BL takeup...        ");
        ui_lcd_set_line(1, "Wait                ");
        ui_lcd_clear_line(2);
        lcd_format_coords_line(lcd_line, sizeof(lcd_line));
        ui_lcd_set_line(3, lcd_line);
        ui_lcd_clear_cursor();
        return;
    }

    SwitchState_t sw = ui_switches_get_state();
    uint8_t mode = fsm_manager_get_mode();
    char feed_txt[16];
    char mpg_txt[16];

    ui_pot_feed_format(feed_txt, sizeof(feed_txt), mode);
    lcd_format_status_line(ui_switches_get_mode_name(sw.mode),
                           feed_txt,
                           submode_short(sw.submode),
                           lcd_line, sizeof(lcd_line));
    ui_lcd_set_line(0, lcd_line);

    char axis = (sw.mpg_axis == AXIS_X) ? 'X' : 'Z';
    snprintf(mpg_txt, sizeof(mpg_txt), "MPG %c %+ld", axis, (long)motion_jog_get_hand(sw.mpg_axis));
    ui_lcd_set_line(1, mpg_txt);
    ui_lcd_clear_line(2);

    lcd_format_coords_line(lcd_line, sizeof(lcd_line));
    ui_lcd_set_line(3, lcd_line);
    ui_lcd_clear_cursor();
}

static void update_lcd(void) {
    if (ui_menu_is_active()) {
        ui_menu_lcd();
    } else {
        update_main_lcd();
    }
}

void setup() {
    hal_init();
    debug_serial_init(SERIAL_BAUD);
    serial_config_init();

    debug_serial_println("");
    debug_serial_println(FIRMWARE_NAME " " FIRMWARE_STAGE " Starting...");

    config_load();
    motion_init();
    timer1_init(STEP_ISR_PERIOD_US);
    motion_jog_init();

    ui_lcd_init();
    ui_switches_init();
    ui_buttons_init();
    limits_ui_init();
    ui_pot_init();
    ui_encoder_init();
    ui_menu_init();
    fsm_manager_init();
    spindle_init();
    encoder_interrupts_init();

    update_lcd();
    ui_lcd_update();

    DBG_INFO("SYS", "INIT", "Stage 2.2f ready");
}

void loop() {
    if (!startup_armed) {
        startup_armed = 1;
        delay(100);
        backlash_startup_begin();
    }

    estop_check();
    backlash_startup_poll();
    ui_buttons_poll();
    ui_switches_poll();
    fsm_manager_poll();
    ui_encoder_poll();
    ui_menu_poll();
    fsm_manager_process();
    spindle_poll();
    serial_config_poll();

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
