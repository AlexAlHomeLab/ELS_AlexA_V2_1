#include "mode_chamfer.h"
#include "../../core/ui/ui_lcd.h"
#include "../../core/motion/planner.h"
#include <math.h>

static uint8_t mode_active = 0;
static float chamfer_angle = 45.0f;
static float chamfer_depth = 1.0f;
static uint8_t chamfer_passes = 3;

void mode_chamfer_enter(void) {
    mode_active = 1;
    ui_lcd_set_line(0, "CHAMFER");
    ui_lcd_set_line(1, "Angle: 45");
    ui_lcd_set_line(2, "Depth: 1.0");
    ui_lcd_set_line(3, "Ready");
}

void mode_chamfer_exit(void) {
    mode_active = 0;
}

void mode_chamfer_process(void) {
    (void)mode_active;
}

void mode_chamfer_ext_start(float start_x, float start_z, float length) {
    (void)length;
    float angle_rad = chamfer_angle * 3.14159f / 180.0f;
    float dx = chamfer_depth * cosf(angle_rad);
    float dz = chamfer_depth * sinf(angle_rad);

    for (uint8_t pass = 0; pass < chamfer_passes; pass++) {
        float ratio = (float)(pass + 1) / chamfer_passes;
        planner_add_move(start_x + dx * ratio, start_z + dz * ratio, 50.0f);
    }
}

void mode_chamfer_int_start(float start_x, float start_z, float length) {
    (void)start_x;
    (void)start_z;
    (void)length;
}
