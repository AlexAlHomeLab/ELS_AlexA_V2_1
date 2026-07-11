#include "mode_sync.h"
#include "../../core/ui/ui_lcd.h"
#include "../../core/ui/ui_pot.h"
#include "../../core/motion/planner.h"
#include "../../core/motion/stepper_gen.h"
#include "../../core/process/spindle_control.h"
#include "../../config/config.h"
#include <stdio.h>

static uint8_t mode_active = 0;
static float feed_per_rev = 0.1f;
static uint8_t sync_active = 0;

void mode_sync_enter(void) {
    mode_active = 1;
    ui_lcd_set_line(0, "SYNC MODE");
    ui_lcd_set_line(1, "Feed/rev: 0.1");
    ui_lcd_set_line(2, "RPM: 0");
    ui_lcd_set_line(3, "Ready");
}

void mode_sync_exit(void) {
    mode_active = 0;
    sync_active = 0;
    planner_stop_all();
}

void mode_sync_process(void) {
    char buf[LCD_COLS + 1];
    if (!mode_active) {
        return;
    }

    feed_per_rev = ui_pot_get_feed_mm_rev();

    uint32_t rpm = spindle_get_rpm();
    if (rpm == 0) {
        ui_lcd_set_line(3, "Spindle stopped");
        return;
    }

    uint32_t max_freq = spindle_get_frequency();
    if (max_freq > 30000) {
        ui_lcd_set_line(3, "Freq too high!");
        sync_active = 0;
        return;
    }

    snprintf(buf, sizeof(buf), "RPM: %lu", (unsigned long)rpm);
    ui_lcd_set_line(2, buf);
}

void mode_sync_start(float feed) {
    if (spindle_get_rpm() == 0) return;

    feed_per_rev = feed;
    uint32_t freq = spindle_get_frequency() * (uint32_t)(feed * STEPS_PER_MM_Z);
    if (freq > 30000) {
        ui_lcd_set_line(3, "Freq too high!");
        return;
    }

    sync_active = 1;
    dds_set_speed(AXIS_Z, freq);
    ui_lcd_set_line(3, "SYNC RUNNING");
}

void mode_sync_stop(void) {
    sync_active = 0;
    planner_stop_all();
    ui_lcd_set_line(3, "Stopped");
}

uint8_t mode_sync_is_active(void) {
    return sync_active;
}
