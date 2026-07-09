#include "mode_thread.h"
#include "thread_tables.h"
#include "../../core/ui/ui_lcd.h"
#include "../../core/motion/planner.h"
#include "../../core/process/spindle_control.h"
#include "../../config/config.h"
#include <stdio.h>

static uint8_t mode_active = 0;
static uint8_t thread_active = 0;
static float current_pitch = 1.0f;
static uint8_t current_thread_index = 0;

void mode_thread_enter(void) {
    mode_active = 1;
    ui_lcd_set_line(0, "THREAD MODE");
    ui_lcd_set_line(1, "Pitch: 1.0mm");
    ui_lcd_set_line(2, "RPM: 0");
    ui_lcd_set_line(3, "Select pitch");
}

void mode_thread_exit(void) {
    mode_active = 0;
    thread_active = 0;
    planner_stop_all();
}

void mode_thread_process(void) {
    char buf[LCD_COLS + 1];
    if (!mode_active) return;
    snprintf(buf, sizeof(buf), "RPM: %lu", (unsigned long)spindle_get_rpm());
    ui_lcd_set_line(2, buf);
}

void mode_thread_select_pitch(uint8_t index) {
    char buf[LCD_COLS + 1];
    if (index >= METRIC_THREADS_COUNT) return;
    current_thread_index = index;
    current_pitch = metric_threads[index].pitch_mm;

    uint32_t rpm = spindle_get_rpm();
    if (rpm > 0) {
        uint32_t freq = (uint32_t)((rpm / 60.0f) * STEPS_PER_MM_Z * current_pitch);
        if (freq > 30000) {
            ui_lcd_set_line(3, "Too fast for pitch!");
            return;
        }
    }

    snprintf(buf, sizeof(buf), "Pitch: %.2f", current_pitch);
    ui_lcd_set_line(1, buf);
    ui_lcd_set_line(3, "Ready");
}

void mode_thread_start(float length, float start_pos, float end_pos) {
    (void)start_pos;
    (void)end_pos;
    if (spindle_get_rpm() == 0) {
        ui_lcd_set_line(3, "Spindle off!");
        return;
    }

    uint32_t freq = (uint32_t)((spindle_get_rpm() / 60.0f) * STEPS_PER_MM_Z * current_pitch);
    if (freq > 30000) {
        ui_lcd_set_line(3, "Too fast!");
        return;
    }

    thread_active = 1;
    uint8_t passes = metric_threads[current_thread_index].passes;
    ui_lcd_set_line(3, "THREADING...");
    for (uint8_t pass = 0; pass < passes; pass++) {
        (void)pass;
        planner_add_move(0, length, 50.0f);
    }
    thread_active = 0;
    ui_lcd_set_line(3, "Done");
}

void mode_thread_stop(void) {
    thread_active = 0;
    planner_stop_all();
    ui_lcd_set_line(3, "Stopped");
}

uint8_t mode_thread_is_active(void) {
    return thread_active;
}
