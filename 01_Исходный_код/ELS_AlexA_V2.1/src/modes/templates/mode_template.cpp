#include "mode_template.h"
#include "../../core/ui/ui_lcd.h"
#include "../../core/fsm/fsm_core.h"

static uint8_t mode_active = 0;

void mode_template_enter(void) {
    mode_active = 1;
    ui_lcd_set_line(0, "TEMPLATE MODE");
    ui_lcd_set_line(1, "Ready");
    fsm_transition(STATE_MANUAL);
}

void mode_template_exit(void) {
    mode_active = 0;
    fsm_transition(STATE_IDLE);
}

void mode_template_process(void) {
    (void)mode_active;
}

void mode_template_cycle_start(void) {
    fsm_transition(STATE_CYCLE);
}

void mode_template_stop(void) {
    fsm_transition(STATE_MANUAL);
}
