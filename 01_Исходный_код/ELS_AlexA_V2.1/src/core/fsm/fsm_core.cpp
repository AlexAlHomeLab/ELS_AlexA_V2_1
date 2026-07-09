#include "fsm_core.h"
#include "../../config/config.h"

static State_t current_state = STATE_IDLE;
static State_t previous_state = STATE_IDLE;
static uint8_t current_mode = 0;
static uint8_t current_submode = SUB_MANUAL;

static const Transition_t transitions[] = {
    {STATE_IDLE, STATE_MANUAL, 0},
    {STATE_MANUAL, STATE_CYCLE, 1},
    {STATE_CYCLE, STATE_PAUSED, 2},
    {STATE_PAUSED, STATE_CYCLE, 3},
    {STATE_MANUAL, STATE_IDLE, 4},
    {STATE_ANY, STATE_ERROR, 5},
    {STATE_ERROR, STATE_IDLE, 6}
};

uint8_t fsm_validate_transition(State_t from, State_t to) {
    for (uint8_t i = 0; i < sizeof(transitions) / sizeof(Transition_t); i++) {
        if ((transitions[i].from == from || transitions[i].from == STATE_ANY) &&
            transitions[i].to == to) {
            return 1;
        }
    }
    return 0;
}

void fsm_transition(State_t new_state) {
    if (!fsm_validate_transition(current_state, new_state)) {
        fsm_error(ERR_INVALID_TRANSITION);
        return;
    }
    previous_state = current_state;
    fsm_exit(current_state);
    current_state = new_state;
    fsm_enter(current_state);
}

State_t fsm_get_state(void) {
    return current_state;
}

void fsm_error(uint8_t error_code) {
    (void)error_code;
    current_state = STATE_ERROR;
    fsm_emergency_stop();
}

void fsm_emergency_stop(void) {
}

void fsm_set_mode(uint8_t mode) {
    current_mode = mode;
}

void fsm_set_submode(uint8_t submode) {
    current_submode = submode;
}

void fsm_enter(State_t state) {
    (void)state;
}

void fsm_exit(State_t state) {
    (void)state;
}

void fsm_process(State_t state) {
    (void)state;
}
