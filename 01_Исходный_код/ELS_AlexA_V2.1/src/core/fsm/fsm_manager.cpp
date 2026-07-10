#include "fsm_manager.h"
#include "fsm_core.h"
#include "../debug/debug_serial.h"
#include "../motion/planner.h"
#include "../motion/backlash.h"
#include "../motion/motion_jog.h"
#include "../motion/planner.h"
#include "../process/estop_control.h"
#include "../ui/ui_switches.h"
#include "../../config/config.h"
#include "../../modes/mode_async/mode_async.h"
#include "../../modes/mode_sync/mode_sync.h"
#include "../../modes/mode_thread/mode_thread.h"
#include "../../modes/mode_chamfer/mode_chamfer.h"
#include "../../modes/mode_cone/mode_cone.h"
#include "../../modes/mode_sphere/mode_sphere.h"
#include "../../modes/mode_divider/mode_divider.h"
#include "../../modes/mode_grind/mode_grind.h"

static uint8_t current_mode = 0;
static uint8_t current_submode = SUB_MANUAL;

static uint8_t mode_index(uint8_t sw_mode) {
    if (sw_mode < 1 || sw_mode > 8) return 0;
    return (uint8_t)(sw_mode - 1);
}

void fsm_manager_set_mode(uint8_t mode) {
    if (mode > 7) mode = 0;
    if (mode == current_mode) return;
    fsm_manager_exit_mode(current_mode);
    current_mode = mode;
    fsm_set_mode(current_mode);
    fsm_manager_enter_mode(current_mode);
    DBG_INFO_VAL("FSM", "MODE", "idx", current_mode);
}

void fsm_manager_set_submode(uint8_t submode) {
    if (submode == current_submode) return;
    current_submode = submode;
    fsm_set_submode(submode);
    DBG_INFO_VAL("FSM", "SUB", "val", submode);
}

void fsm_manager_enter_mode(uint8_t mode) {
    switch (mode) {
        case 0: mode_async_enter(); break;
        case 1: mode_sync_enter(); break;
        case 2: mode_thread_enter(); break;
        case 3: mode_chamfer_enter(); break;
        case 4: mode_cone_enter(); break;
        case 5: mode_sphere_enter(); break;
        case 6: mode_divider_enter(); break;
        case 7: mode_grind_enter(); break;
        default: break;
    }
    if (fsm_get_state() == STATE_IDLE) {
        fsm_transition(STATE_MANUAL);
    }
}

void fsm_manager_exit_mode(uint8_t mode) {
    switch (mode) {
        case 0: mode_async_exit(); break;
        case 1: mode_sync_exit(); break;
        case 2: mode_thread_exit(); break;
        case 3: mode_chamfer_exit(); break;
        case 4: mode_cone_exit(); break;
        case 5: mode_sphere_exit(); break;
        case 6: mode_divider_exit(); break;
        case 7: mode_grind_exit(); break;
        default: break;
    }
}

void fsm_manager_init(void) {
    SwitchState_t sw = ui_switches_get_state();
    current_mode = mode_index(sw.mode);
    current_submode = sw.submode;
    fsm_set_mode(current_mode);
    fsm_set_submode(current_submode);
    fsm_manager_enter_mode(current_mode);
}

void fsm_manager_poll(void) {
    if (estop_is_triggered()) return;

    SwitchState_t sw = ui_switches_get_state();
    uint8_t mode = mode_index(sw.mode);

    if (mode != current_mode) {
        fsm_manager_set_mode(mode);
    }
    if (sw.submode != current_submode) {
        fsm_manager_set_submode(sw.submode);
    }
}

void fsm_manager_process(void) {
    State_t st;

    planner_process();

    if (planner_startup_busy()) {
        return;
    }

    st = fsm_get_state();

    if (st == STATE_ERROR) {
        return;
    }

    if (st == STATE_MANUAL) {
        motion_jog_joy_poll();
        motion_jog_poll();
    }

    switch (current_mode) {
        case 0: mode_async_process(); break;
        case 1: mode_sync_process(); break;
        case 2: mode_thread_process(); break;
        case 3: mode_chamfer_process(); break;
        case 4: mode_cone_process(); break;
        case 5: mode_sphere_process(); break;
        case 6: mode_divider_process(); break;
        case 7: mode_grind_process(); break;
        default: break;
    }
}

uint8_t fsm_manager_get_mode(void) {
    return current_mode;
}

uint8_t fsm_manager_get_submode(void) {
    return current_submode;
}
