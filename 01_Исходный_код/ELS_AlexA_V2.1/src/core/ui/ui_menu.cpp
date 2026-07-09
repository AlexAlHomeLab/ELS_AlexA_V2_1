#include "ui_menu.h"
#include "ui_lcd.h"
#include "ui_buttons.h"
#include "ui_pot.h"
#include "../fsm/fsm_core.h"
#include "../../config/config.h"

static const char *mode_names[] = {
    "ASYNC", "SYNC", "CHAMFER", "THREAD", "CONE L",
    "CONE R", "DIVIDER", "SPHERE", "GRIND"
};

static uint8_t current_mode = 0;
static uint8_t current_submode = SUB_MANUAL;

void ui_menu_init(void) {
    ui_menu_update();
}

void ui_menu_update(void) {
    ui_lcd_set_line(0, "ELS AlexA V2.1");
    ui_lcd_set_line(1, mode_names[current_mode]);
    switch (current_submode) {
        case SUB_MANUAL: ui_lcd_set_line(2, "MAN"); break;
        case SUB_EXT: ui_lcd_set_line(2, "EXT"); break;
        case SUB_INT: ui_lcd_set_line(2, "INT"); break;
        default: ui_lcd_set_line(2, "---"); break;
    }
}

void ui_menu_handle_navigation(void) {
    ButtonState_t btn = ui_buttons_get_state();

    if (btn.left) {
        current_mode = (current_mode > 0) ? current_mode - 1 : 8;
        fsm_set_mode(current_mode);
        ui_menu_update();
    }
    if (btn.right) {
        current_mode = (current_mode < 8) ? current_mode + 1 : 0;
        fsm_set_mode(current_mode);
        ui_menu_update();
    }
    if (btn.select) {
        current_submode = ui_pot_get_submode();
        fsm_set_submode(current_submode);
        ui_menu_update();
    }
    if (btn.select_hold) {
        ui_menu_settings();
    }
}

void ui_menu_settings(void) {
    ui_lcd_clear();
    ui_lcd_set_line(0, "SETTINGS");
    ui_lcd_set_line(1, "Speed:");
    ui_lcd_set_line(2, "Accel:");
}

void ui_menu_set_mode(uint8_t mode) {
    current_mode = mode;
    ui_menu_update();
}

void ui_menu_set_submode(uint8_t submode) {
    current_submode = submode;
    ui_menu_update();
}
