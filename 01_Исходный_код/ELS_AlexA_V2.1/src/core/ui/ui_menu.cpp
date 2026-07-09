#include "ui_menu.h"
#include "ui_buttons.h"
#include "ui_lcd.h"
#include "../../config/config.h"
#include "../../config/config_storage.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_pins.h"
#include <Arduino.h>
#include <stdio.h>

static uint8_t menu_active = 0;
static uint8_t param_idx = 0;
static uint8_t edit_feed_max = CONFIG_FEED_MAX_DEFAULT;
static uint8_t edit_buzzer = CONFIG_BUZZER_DEFAULT;
static uint8_t open_hold_latched = 0;
static uint8_t cancel_hold_latched = 0;

static void ui_buzzer_beep(void) {
    if (!edit_buzzer) return;
    tone(BUZZER_PIN, 2500, 40);
}

void ui_menu_init(void) {
    menu_active = 0;
    param_idx = 0;
    edit_feed_max = config_get_feed_max();
    edit_buzzer = config_get_buzzer_on();
}

uint8_t ui_menu_is_active(void) {
    return menu_active;
}

void ui_menu_lcd(void) {
    char line[LCD_COLS + 1];

    ui_lcd_set_line(0, "SETTINGS");
    snprintf(line, sizeof(line), "%cFeed max: %u%%", param_idx == 0 ? '>' : ' ', edit_feed_max);
    ui_lcd_set_line(1, line);
    snprintf(line, sizeof(line), "%cBuzzer: %s", param_idx == 1 ? '>' : ' ', edit_buzzer ? "ON" : "OFF");
    ui_lcd_set_line(2, line);
    ui_lcd_set_line(3, "HldSel=cancel");
}

static void menu_open(void) {
    menu_active = 1;
    param_idx = 0;
    edit_feed_max = config_get_feed_max();
    edit_buzzer = config_get_buzzer_on();
    DBG_INFO("UI", "MENU", "open");
}

static void menu_save_exit(void) {
    config_set_feed_max(edit_feed_max);
    config_set_buzzer_on(edit_buzzer);
    config_save();
    menu_active = 0;
    ui_buzzer_beep();
    DBG_INFO_VAL("UI", "MENU", "feed", edit_feed_max);
}

static void menu_cancel_exit(void) {
    menu_active = 0;
    DBG_INFO("UI", "MENU", "cancel");
}

static void menu_adjust(int8_t dir) {
    if (param_idx == 0) {
        if (dir > 0 && edit_feed_max < 100) edit_feed_max += 5;
        if (dir < 0 && edit_feed_max > 10) edit_feed_max -= 5;
    } else if (dir != 0) {
        edit_buzzer = (dir > 0) ? 1 : 0;
    }
}

void ui_menu_poll(void) {
    ButtonState_t st = ui_buttons_get_state();
    ButtonClicks_t cl = ui_buttons_get_clicks();

    if (!menu_active) {
        if (st.select_hold) {
            if (!open_hold_latched) {
                open_hold_latched = 1;
                menu_open();
            }
        } else {
            open_hold_latched = 0;
        }
        return;
    }

    if (st.select_hold) {
        if (!cancel_hold_latched) {
            cancel_hold_latched = 1;
            menu_cancel_exit();
        }
        return;
    }
    cancel_hold_latched = 0;

    if (cl.select) {
        menu_save_exit();
        return;
    }
    if (cl.left && param_idx > 0) param_idx--;
    if (cl.right && param_idx < 1) param_idx++;
    if (cl.up) menu_adjust(1);
    if (cl.down) menu_adjust(-1);
}
