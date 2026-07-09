#include "mode_divider.h"
#include "../../core/ui/ui_lcd.h"
#include "../../core/motion/planner.h"
#include "../../config/config.h"

static uint8_t mode_active = 0;
static uint16_t parts = 36;
static float current_angle = 0;
static uint16_t current_part = 0;

void mode_divider_enter(void) {
    mode_active = 1;
    ui_lcd_set_line(0, "DIVIDER");
    ui_lcd_set_line(1, "Parts: 36");
    ui_lcd_set_line(2, "Angle: 0.0");
    ui_lcd_set_line(3, "Part: 0/36");
}

void mode_divider_exit(void) {
    mode_active = 0;
}

void mode_divider_process(void) {
    (void)mode_active;
}

void mode_divider_set_parts(uint16_t n) {
    parts = n;
    current_angle = 0;
    current_part = 0;
}

void mode_divider_next(void) {
    if (current_part >= parts) return;
    current_part++;
    current_angle = (float)current_part / parts * 360.0f;
    float steps = current_angle * STEPS_PER_MM_X / 360.0f;
    planner_add_move(steps, 0, 100.0f);
}

void mode_divider_reset(void) {
    current_angle = 0;
    current_part = 0;
}

float mode_divider_get_angle(void) {
    return current_angle;
}
