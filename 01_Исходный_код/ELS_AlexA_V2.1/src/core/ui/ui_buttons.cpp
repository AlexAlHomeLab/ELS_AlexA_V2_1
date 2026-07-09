#include "../../config/config.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_pins.h"
#include "../motion/limits.h"
#include "ui_buttons.h"
#include "ui_io.h"

#include <Arduino.h>
#include <string.h>
#include <EncButton.h>

static ButtonT<BTN_LEFT_PIN> btn_left;
static ButtonT<BTN_RIGHT_PIN> btn_right;
static ButtonT<BTN_UP_PIN> btn_up;
static ButtonT<BTN_DOWN_PIN> btn_down;
static ButtonT<BTN_SELECT_PIN> btn_select;
static ButtonT<JOY_LEFT_PIN> btn_joy_left;
static ButtonT<JOY_RIGHT_PIN> btn_joy_right;
static ButtonT<JOY_UP_PIN> btn_joy_up;
static ButtonT<JOY_DOWN_PIN> btn_joy_down;
static ButtonT<JOY_RAPID_PIN> btn_joy_rapid;
static ButtonT<LIMIT_LEFT_PIN> btn_limit_left;
static ButtonT<LIMIT_FRONT_PIN> btn_limit_front;
static ButtonT<LIMIT_RIGHT_PIN> btn_limit_right;
static ButtonT<LIMIT_REAR_PIN> btn_limit_rear;

static ButtonState_t btn_state;
static ButtonClicks_t btn_clicks;

static void set_btn_level(VirtButton &btn) {
    btn.setBtnLevel(LOW);
}

static void on_menu_click(VirtButton &btn, uint8_t *flag, const char *name) {
    if (btn.click()) {
        *flag = 1;
        DBG_INFO("UI", "BTN", name);
    }
    if (btn.hold()) {
        DBG_INFO("UI", "HOLD", name);
    }
}

static void log_btn_event(const char *name, VirtButton &btn) {
    if (btn.click()) {
        DBG_INFO("UI", "BTN", name);
    }
    if (btn.hold()) {
        DBG_INFO("UI", "HOLD", name);
    }
}

void ui_buttons_init(void) {
    set_btn_level(btn_left);
    set_btn_level(btn_right);
    set_btn_level(btn_up);
    set_btn_level(btn_down);
    set_btn_level(btn_select);
    set_btn_level(btn_joy_left);
    set_btn_level(btn_joy_right);
    set_btn_level(btn_joy_up);
    set_btn_level(btn_joy_down);
    set_btn_level(btn_joy_rapid);
    set_btn_level(btn_limit_left);
    set_btn_level(btn_limit_front);
    set_btn_level(btn_limit_right);
    set_btn_level(btn_limit_rear);
    memset(&btn_state, 0, sizeof(btn_state));
    memset(&btn_clicks, 0, sizeof(btn_clicks));
}

static void log_limit_event(const char *name, VirtButton &btn, uint8_t idx) {
    if (btn.click()) {
        DBG_INFO("UI", "BTN", name);
        limits_ui_on_click(idx);
    }
    if (btn.hold()) {
        DBG_INFO("UI", "HOLD", name);
    }
}

void ui_buttons_poll(void) {
    memset(&btn_clicks, 0, sizeof(btn_clicks));

    btn_left.tick();
    btn_right.tick();
    btn_up.tick();
    btn_down.tick();
    btn_select.tick();
    btn_joy_left.tick();
    btn_joy_right.tick();
    btn_joy_up.tick();
    btn_joy_down.tick();
    btn_joy_rapid.tick();
    btn_limit_left.tick();
    btn_limit_front.tick();
    btn_limit_right.tick();
    btn_limit_rear.tick();

    on_menu_click(btn_left, &btn_clicks.left, "Left");
    on_menu_click(btn_right, &btn_clicks.right, "Right");
    on_menu_click(btn_up, &btn_clicks.up, "Up");
    on_menu_click(btn_down, &btn_clicks.down, "Down");
    on_menu_click(btn_select, &btn_clicks.select, "Select");
    log_btn_event("JoyL", btn_joy_left);
    log_btn_event("JoyR", btn_joy_right);
    log_btn_event("JoyU", btn_joy_up);
    log_btn_event("JoyD", btn_joy_down);
    log_btn_event("Rapid", btn_joy_rapid);
    log_limit_event("LimL", btn_limit_left, 0);
    log_limit_event("LimF", btn_limit_front, 1);
    log_limit_event("LimR", btn_limit_right, 2);
    log_limit_event("LimRe", btn_limit_rear, 3);

    btn_state.left = btn_left.pressing();
    btn_state.right = btn_right.pressing();
    btn_state.up = btn_up.pressing();
    btn_state.down = btn_down.pressing();
    btn_state.select = btn_select.pressing();
    btn_state.select_hold = btn_select.holding();
    btn_state.joy_left = btn_joy_left.pressing();
    btn_state.joy_right = btn_joy_right.pressing();
    btn_state.joy_up = btn_joy_up.pressing();
    btn_state.joy_down = btn_joy_down.pressing();
    btn_state.joy_rapid = btn_joy_rapid.pressing();
    btn_state.limit_left = btn_limit_left.pressing();
    btn_state.limit_front = btn_limit_front.pressing();
    btn_state.limit_right = btn_limit_right.pressing();
    btn_state.limit_rear = btn_limit_rear.pressing();
}

ButtonState_t ui_buttons_get_state(void) {
    return btn_state;
}

ButtonClicks_t ui_buttons_get_clicks(void) {
    return btn_clicks;
}
