#include "../../config/config.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_pins.h"
#include "../motion/limits.h"
#include "../motion/motion_jog.h"
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
static uint8_t feed_joy_click_flag;

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

static uint8_t joy_feed_dir(uint8_t *axis, int8_t *sign) {
    uint8_t z = 0;
    uint8_t x = 0;
    int8_t zs = 0;
    int8_t xs = 0;

    if (btn_joy_left.read()) { z = 1; zs = -1; }
    else if (btn_joy_right.read()) { z = 1; zs = 1; }
    if (btn_joy_up.read()) { x = 1; xs = 1; }
    else if (btn_joy_down.read()) { x = 1; xs = -1; }
    if (z && x) return 0;
    if (z) { *axis = AXIS_Z; *sign = zs; return 1; }
    if (x) { *axis = AXIS_X; *sign = xs; return 1; }
    return 0;
}

static void log_limit_event(const char *name, VirtButton &btn, uint8_t idx) {
    if (btn.hold()) {
        DBG_INFO("UI", "HOLD", name);
        if (idx == 0) {
            uint8_t axis;
            int8_t sign;
            uint8_t lim_idx;
            int32_t target;
            if (joy_feed_dir(&axis, &sign) &&
                limits_ui_go_target_dir(axis, sign, &lim_idx, &target)) {
                DBG_INFO("UI", "LATCH", name);
                motion_jog_go_limit_latch(lim_idx);
                return;
            }
        }
        limits_ui_on_hold(idx);
        return;
    }
    if (btn.click()) {
        if (btn_joy_rapid.read()) {
            DBG_INFO("UI", "GOLIM", name);
            motion_jog_go_limit(idx);
            return;
        }
        DBG_INFO("UI", "BTN", name);
        limits_ui_on_click(idx);
    }
}

static uint8_t limit_pressed_idx(void) {
    if (btn_limit_left.pressing()) return 0;
    if (btn_limit_front.pressing()) return 1;
    if (btn_limit_right.pressing()) return 2;
    if (btn_limit_rear.pressing()) return 3;
    return 0xFF;
}

static void log_rapid_event(void) {
    if (btn_joy_rapid.click()) {
        uint8_t idx = limit_pressed_idx();
        if (idx != 0xFF) {
            DBG_INFO("UI", "GOLIM", "Rapid");
            motion_jog_go_limit(idx);
            return;
        }
        DBG_INFO("UI", "BTN", "Rapid");
    }
    if (btn_joy_rapid.hold()) {
        DBG_INFO("UI", "HOLD", "Rapid");
    }
}

void ui_buttons_poll(void) {
    memset(&btn_clicks, 0, sizeof(btn_clicks));
    feed_joy_click_flag = 0;

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
    if (btn_joy_left.click() || btn_joy_right.click() ||
        btn_joy_up.click() || btn_joy_down.click()) {
        feed_joy_click_flag = 1;
    }
    log_rapid_event();
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
    /* Джойстик: read() — сырое состояние пина для удержания (jog).
     * pressing() ломается после menu_save_all(): EEPROM блокирует loop,
     * millis не тикает, EncButton теряет EB_PRS при удержании. */
    btn_state.joy_left = btn_joy_left.read();
    btn_state.joy_right = btn_joy_right.read();
    btn_state.joy_up = btn_joy_up.read();
    btn_state.joy_down = btn_joy_down.read();
    btn_state.joy_rapid = btn_joy_rapid.read();
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

uint8_t ui_buttons_feed_joy_click(void) {
    return feed_joy_click_flag;
}

uint8_t ui_buttons_feed_joy_on(void) {
    return btn_state.joy_left || btn_state.joy_right ||
           btn_state.joy_up || btn_state.joy_down;
}

void ui_buttons_reset_joy(void) {
    btn_joy_left.reset();
    btn_joy_right.reset();
    btn_joy_up.reset();
    btn_joy_down.reset();
    btn_joy_rapid.reset();
}
