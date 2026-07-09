#include "fsm_cycles.h"
#include "fsm_core.h"
#include "../motion/limits.h"
#include "../motion/stepper_gen.h"
#include "../motion/planner.h"
#include "../../config/config.h"

typedef struct {
    uint8_t active;
    uint8_t type;
    uint8_t axis;
    float depth;
    uint8_t passes;
    uint8_t current_pass;
    uint8_t phase;
    float start_x;
    float start_z;
    float end_x;
    float end_z;
} CycleState_t;

static CycleState_t cycle = {0};

void fsm_cycle_start(uint8_t type, uint8_t axis, float depth, uint8_t passes) {
    if (!limits_hardware_check()) {
        fsm_error(ERR_LIMIT);
        return;
    }
    cycle.active = 1;
    cycle.type = type;
    cycle.axis = axis;
    cycle.depth = depth;
    cycle.passes = passes;
    cycle.current_pass = 0;
    cycle.phase = 0;
    cycle.start_x = (float)dds_get_position(AXIS_X);
    cycle.start_z = (float)dds_get_position(AXIS_Z);
    fsm_transition(STATE_CYCLE);
}

void fsm_cycle_process(void) {
    if (!cycle.active) return;
    switch (cycle.type) {
        case CYCLE_EXT: fsm_cycle_ext_process(); break;
        case CYCLE_INT: fsm_cycle_int_process(); break;
        default: break;
    }
}

void fsm_cycle_ext_process(void) {
}

void fsm_cycle_int_process(void) {
}

void fsm_cycle_stop(void) {
    cycle.active = 0;
    planner_stop_all();
    fsm_transition(STATE_MANUAL);
}

void fsm_cycle_pause(void) {
    if (cycle.active) {
        fsm_transition(STATE_PAUSED);
    }
}

void fsm_cycle_resume(void) {
    if (fsm_get_state() == STATE_PAUSED) {
        fsm_transition(STATE_CYCLE);
    }
}
