#include "mode_sphere.h"
#include "../../core/ui/ui_lcd.h"
#include "../../core/motion/planner.h"
#include <math.h>

static uint8_t mode_active = 0;

void mode_sphere_enter(void) {
    mode_active = 1;
    ui_lcd_set_line(0, "SPHERE");
}

void mode_sphere_exit(void) {
    mode_active = 0;
}

void mode_sphere_process(void) {
    (void)mode_active;
}

void mode_sphere_start(float center_x, float center_z, float radius, float depth) {
    (void)depth;
    const uint16_t segments = 36;
    for (uint16_t i = 0; i <= segments; i++) {
        float angle = (float)i / segments * 3.14159f * 2.0f;
        float x = center_x + radius * cosf(angle);
        float z = center_z + radius * sinf(angle);
        planner_add_move(x, z, 30.0f);
    }
}
