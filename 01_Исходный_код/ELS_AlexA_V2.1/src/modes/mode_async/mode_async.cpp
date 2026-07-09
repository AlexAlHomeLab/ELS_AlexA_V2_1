#include "mode_async.h"
#include "../../config/config.h"
#include "../../core/ui/ui_lcd.h"
#include "../../core/ui/ui_buttons.h"
#include "../../core/motion/planner.h"
#include "../../core/motion/limits.h"
#include "../../core/fsm/fsm_core.h"

static uint8_t mode_active = 0;
static uint8_t submode = SUB_MANUAL;
static float feed_speed = 0;

void mode_async_enter(void) {
    mode_active = 1;
    submode = SUB_MANUAL;
    ui_lcd_set_line(0, "ASYNC MODE");
    ui_lcd_set_line(1, "MAN");
    ui_lcd_set_line(2, "X:     Z:");
    ui_lcd_set_line(3, "Feed:");
}

void mode_async_exit(void) {
    mode_active = 0;
    planner_stop_all();
}

void mode_async_handle_manual(void) {
    ButtonState_t btn = ui_buttons_get_state();
    if (btn.left) planner_add_move(-1.0f, 0, feed_speed);
    if (btn.right) planner_add_move(1.0f, 0, feed_speed);
    if (btn.up) planner_add_move(0, 1.0f, feed_speed);
    if (btn.down) planner_add_move(0, -1.0f, feed_speed);
}

void mode_async_cycle_ext_start(void) {
    submode = SUB_EXT;
    ui_lcd_set_line(1, "EXT CYCLE");
    if (!limits_hardware_check()) {
        fsm_error(ERR_LIMIT);
        return;
    }
    fsm_transition(STATE_CYCLE);
}

void mode_async_cycle_int_start(void) {
    submode = SUB_INT;
    ui_lcd_set_line(1, "INT CYCLE");
    if (!limits_hardware_check()) {
        fsm_error(ERR_LIMIT);
        return;
    }
    fsm_transition(STATE_CYCLE);
}

void mode_async_process(void) {
}

void mode_async_cycle_process(void) {
}

void mode_async_set_feed(float speed) {
    feed_speed = speed;
}

void mode_async_set_depth(float depth) {
    (void)depth;
}

void mode_async_set_passes(uint8_t passes) {
    (void)passes;
}

uint8_t mode_async_is_active(void) {
    return mode_active;
}
