#include "mode_cone.h"
#include "../../core/ui/ui_lcd.h"
#include "../../core/motion/planner.h"
#include <math.h>

static uint8_t mode_active = 0;

void mode_cone_enter(void) {
    mode_active = 1;
    ui_lcd_set_line(0, "CONE");
}

void mode_cone_exit(void) {
    mode_active = 0;
}

void mode_cone_process(void) {
    (void)mode_active;
}

void mode_cone_left_start(float start_x, float start_z, float angle, float length) {
    float angle_rad = angle * 3.14159f / 180.0f;
    float dx = length * tanf(angle_rad);
    planner_add_move(start_x + dx, start_z + length, 50.0f);
}

void mode_cone_right_start(float start_x, float start_z, float angle, float length) {
    float angle_rad = angle * 3.14159f / 180.0f;
    float dx = -length * tanf(angle_rad);
    planner_add_move(start_x + dx, start_z + length, 50.0f);
}
