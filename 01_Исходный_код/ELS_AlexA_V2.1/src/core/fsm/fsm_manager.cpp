#include "fsm_manager.h"
#include "fsm_core.h"
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

void fsm_manager_set_mode(uint8_t mode) {
    current_mode = mode;
    fsm_manager_exit_mode(current_mode);
    fsm_manager_enter_mode(mode);
}

void fsm_manager_enter_mode(uint8_t mode) {
    switch (mode) {
        case 0: mode_async_enter(); break;
        case 1: mode_sync_enter(); break;
        case 2: mode_chamfer_enter(); break;
        case 3: mode_thread_enter(); break;
        case 4: mode_cone_enter(); break;
        case 5: mode_cone_enter(); break;
        case 6: mode_divider_enter(); break;
        case 7: mode_sphere_enter(); break;
        case 8: mode_grind_enter(); break;
        default: break;
    }
    fsm_transition(STATE_MANUAL);
}

void fsm_manager_exit_mode(uint8_t mode) {
    switch (mode) {
        case 0: mode_async_exit(); break;
        case 1: mode_sync_exit(); break;
        case 2: mode_chamfer_exit(); break;
        case 3: mode_thread_exit(); break;
        case 4: mode_cone_exit(); break;
        case 5: mode_cone_exit(); break;
        case 6: mode_divider_exit(); break;
        case 7: mode_sphere_exit(); break;
        case 8: mode_grind_exit(); break;
        default: break;
    }
}

void fsm_manager_process(void) {
    switch (current_mode) {
        case 0: mode_async_process(); break;
        case 1: mode_sync_process(); break;
        case 2: mode_chamfer_process(); break;
        case 3: mode_thread_process(); break;
        case 4: mode_cone_process(); break;
        case 5: mode_cone_process(); break;
        case 6: mode_divider_process(); break;
        case 7: mode_sphere_process(); break;
        case 8: mode_grind_process(); break;
        default: break;
    }
}

uint8_t fsm_manager_get_mode(void) {
    return current_mode;
}

uint8_t fsm_manager_get_submode(void) {
    return current_submode;
}
