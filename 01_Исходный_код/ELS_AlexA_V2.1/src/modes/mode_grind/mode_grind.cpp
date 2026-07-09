#include "mode_grind.h"
#include "mode_grind_cfg.h"
#include "../../core/motion/planner.h"
#include "../../core/process/spindle_control.h"
#include "../../core/ui/ui_lcd.h"
#include "../../core/fsm/fsm_core.h"
#include "../../core/motion/limits.h"
#include "../../config/config.h"
#include <stdio.h>

static uint8_t mode_active = 0;
static uint8_t cycle_active = 0;
static float grind_depth = GRIND_DEFAULT_DEPTH;
static uint8_t grind_passes = GRIND_DEFAULT_PASSES;
static uint8_t current_pass = 0;
static uint8_t cycle_phase = 0;

void mode_grind_enter(void) {
    mode_active = 1;
    cycle_active = 0;
    current_pass = 0;
    cycle_phase = 0;
    ui_lcd_set_line(0, "GRIND MODE");
    ui_lcd_set_line(1, "Depth: 0.010mm");
    ui_lcd_set_line(2, "Passes: 5");
    ui_lcd_set_line(3, "Ready");
    fsm_transition(STATE_MANUAL);
}

void mode_grind_exit(void) {
    mode_active = 0;
    cycle_active = 0;
    planner_stop_all();
    fsm_transition(STATE_IDLE);
}

void mode_grind_process(void) {
    char buf[LCD_COLS + 1];
    if (!mode_active) return;
    if (cycle_active) {
        snprintf(buf, sizeof(buf), "Pass: %d/%d", current_pass, grind_passes);
        ui_lcd_set_line(3, buf);
    }
}

void mode_grind_start(float depth, uint8_t passes) {
    if (depth < GRIND_MIN_DEPTH) depth = GRIND_MIN_DEPTH;
    if (depth > GRIND_MAX_DEPTH) depth = GRIND_MAX_DEPTH;
    if (passes < GRIND_MIN_PASSES) passes = GRIND_MIN_PASSES;
    if (passes > GRIND_MAX_PASSES) passes = GRIND_MAX_PASSES;
    grind_depth = depth;
    grind_passes = passes;
    current_pass = 0;
    cycle_phase = 0;
    if (!limits_hardware_check()) {
        fsm_error(ERR_LIMIT);
        return;
    }
    if (spindle_get_rpm() == 0) {
        ui_lcd_set_line(3, "Spindle off!");
        return;
    }
    cycle_active = 1;
    ui_lcd_set_line(3, "GRINDING...");
    fsm_transition(STATE_CYCLE);
}

void mode_grind_stop(void) {
    cycle_active = 0;
    planner_stop_all();
    ui_lcd_set_line(3, "Stopped");
    fsm_transition(STATE_MANUAL);
}

uint8_t mode_grind_is_active(void) {
    return cycle_active;
}

void mode_grind_set_depth(float depth) {
    char buf[LCD_COLS + 1];
    if (depth < GRIND_MIN_DEPTH) depth = GRIND_MIN_DEPTH;
    if (depth > GRIND_MAX_DEPTH) depth = GRIND_MAX_DEPTH;
    grind_depth = depth;
    snprintf(buf, sizeof(buf), "Depth: %.3fmm", grind_depth);
    ui_lcd_set_line(1, buf);
}

void mode_grind_set_passes(uint8_t passes) {
    char buf[LCD_COLS + 1];
    if (passes < GRIND_MIN_PASSES) passes = GRIND_MIN_PASSES;
    if (passes > GRIND_MAX_PASSES) passes = GRIND_MAX_PASSES;
    grind_passes = passes;
    snprintf(buf, sizeof(buf), "Passes: %d", grind_passes);
    ui_lcd_set_line(2, buf);
}

float mode_grind_get_depth(void) {
    return grind_depth;
}

uint8_t mode_grind_get_passes(void) {
    return grind_passes;
}

void mode_grind_cycle_process(void) {
    if (!cycle_active) return;
    switch (cycle_phase) {
        case 0:
            planner_add_move(0, -grind_depth, GRIND_MIN_FEED);
            cycle_phase = 1;
            break;
        case 1:
            planner_add_move(0, 10.0f, GRIND_MAX_FEED / 2);
            cycle_phase = 2;
            break;
        case 2:
            planner_add_move(0, grind_depth, GRIND_MAX_FEED);
            cycle_phase = 3;
            break;
        case 3:
            planner_add_move(0, -10.0f, GRIND_MAX_FEED);
            current_pass++;
            cycle_phase = 0;
            if (current_pass >= grind_passes) {
                cycle_active = 0;
                ui_lcd_set_line(3, "Done");
                fsm_transition(STATE_MANUAL);
            }
            break;
        default:
            cycle_phase = 0;
            break;
    }
}
