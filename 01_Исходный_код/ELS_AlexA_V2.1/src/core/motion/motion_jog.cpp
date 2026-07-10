#include "motion_jog.h"
#include "motion_control.h"
#include "planner.h"
#include "stepper_gen.h"
#include "backlash.h"
#include "../../config/config.h"
#include "../../config/config_machine.h"
#include "../debug/debug_serial.h"
#include "../process/estop_control.h"
#include "../motion/limits.h"
#include "../ui/ui_buttons.h"
#include "../ui/ui_encoder.h"
#include "../ui/ui_pot.h"
#include "../ui/ui_switches.h"

#include <Arduino.h>

#define JOY_STEP_MS 20UL
#define JOY_LOOKAHEAD 8
#define MPG_MAX_TICKS 24
#define MPG_LOOKAHEAD 4
#define MPG_IDLE_STOP_MS 80UL
#define MPG_BATCH_MS 8UL        /* ponytail: wait for all detent ticks before first exec */
#define MPG_IDLE_DECEL_MS 250UL /* ponytail: session reset only; no decel between detents */
#define MPG_REV_IGNORE_TICKS 3

static int32_t hand_pos[2];
static uint8_t joy_z_on;
static uint8_t joy_x_on;
static unsigned long joy_tick_ms;
static uint8_t go_lim_active;
static uint8_t go_lim_axis;
static int32_t go_lim_target;
static uint8_t go_lim_stop_joy;
static uint8_t go_lim_latch;
static uint8_t go_lim_joy_arm;
static uint8_t mpg_active;
static unsigned long mpg_last_ms;
static uint8_t mpg_startup_sync = 1;
static int32_t mpg_cmd[2];
static uint8_t mpg_axis_last;
static uint8_t mpg_rev_cnt;
static int8_t mpg_dir_lock;
static unsigned long mpg_batch_ms;

static void go_limit_stop(void) {
    if (!go_lim_active) return;
    go_lim_active = 0;
    go_lim_latch = 0;
    go_lim_joy_arm = 0;
    dds_motion_stop();
    dds_set_target(go_lim_axis, dds_get_position(go_lim_axis));
}

static void hand_reset_axis(uint8_t axis) {
    hand_pos[axis] = 0;
    ui_encoder_reset_mpg();
    DBG_VERBOSE_VAL("JOG", "HAND", axis == AXIS_X ? "Xrst" : "Zrst", 0);
}

static int32_t jog_steps_from_delta(uint8_t axis, int32_t delta, uint8_t mpg_scale, uint8_t rapid) {
    int32_t spm;
    int32_t step;

    if (delta == 0) return 0;
    if (delta > 1) delta = 1;
    if (delta < -1) delta = -1;

    spm = (int32_t)((axis == AXIS_X) ? STEPS_PER_MM_X : STEPS_PER_MM_Z);

    if (rapid) {
        step = spm / 10;
        if (step < 1) step = 1;
        return delta > 0 ? step : -step;
    }
    if (mpg_scale == 1) {
        step = spm / 100;
        if (step < 1) step = 1;
        return delta > 0 ? step : -step;
    }
    return delta;
}

static float jog_speed_mm_min(uint8_t axis, uint8_t rapid) {
    float mm_min;

    if (rapid) {
        mm_min = (float)config_get_rapid_speed_mm_min(axis);
    } else {
        mm_min = ui_pot_get_jog_mm_min();
        if (mm_min < 10.0f) mm_min = 10.0f;
        float cap = (float)config_get_max_speed_mm_min(axis);
        if (mm_min > cap) mm_min = cap;
    }
    return mm_min;
}

static float jog_speed_dual_mm_min(uint8_t rapid) {
    float sx = jog_speed_mm_min(AXIS_X, rapid);
    float sz = jog_speed_mm_min(AXIS_Z, rapid);
    return (sx > sz) ? sx : sz;
}

static int32_t joy_chunk(uint8_t axis, uint8_t rapid) {
    float mm_min = jog_speed_mm_min(axis, rapid);
    float spm = (axis == AXIS_X) ? STEPS_PER_MM_X : STEPS_PER_MM_Z;
    int32_t chunk = (int32_t)((mm_min * spm) / (60.0f * (1000.0f / (float)JOY_STEP_MS)));
    if (chunk < 1) chunk = 1;
    return chunk;
}

static int32_t jog_clamp_target(uint8_t axis, int32_t target, uint8_t rapid) {
    if (rapid) return target;
    return limits_clamp_steps(axis, target);
}

static void mpg_sync_cmd(void) {
    mpg_cmd[AXIS_X] = dds_get_position(AXIS_X);
    mpg_cmd[AXIS_Z] = dds_get_position(AXIS_Z);
}

static int32_t mpg_runway(uint8_t axis, int8_t sign, uint8_t mpg_scale, uint8_t rapid) {
    int32_t step;

    if (sign == 0) return 0;
    step = jog_steps_from_delta(axis, sign, mpg_scale, rapid);
    if (step < 0) step = -step;
    return step * (int32_t)MPG_LOOKAHEAD;
}

static int8_t mpg_motion_sign(uint8_t axis) {
    int32_t pos = dds_get_position(axis);
    int32_t err = mpg_cmd[axis] - pos;

    if (err > 0) return 1;
    if (err < 0) return -1;
    if (!dds_motion_busy()) return 0;

    {
        int32_t tgt = dds_get_target(axis);
        if (tgt > pos) return 1;
        if (tgt < pos) return -1;
    }
    return 0;
}

static void mpg_motion_abort(uint8_t axis) {
    dds_motion_stop();
    planner_jog_stop();
    mpg_cmd[axis] = dds_get_position(axis);
    mpg_active = 0;
    mpg_dir_lock = 0;
    mpg_rev_cnt = 0;
    mpg_batch_ms = 0;
}

static void mpg_sync_overshoot(uint8_t axis) {
    int32_t pos = dds_get_position(axis);

    if (mpg_dir_lock > 0 && pos > mpg_cmd[axis]) {
        mpg_cmd[axis] = pos;
    } else if (mpg_dir_lock < 0 && pos < mpg_cmd[axis]) {
        mpg_cmd[axis] = pos;
    }
}

static void mpg_planner_commit(uint8_t axis, const SwitchState_t *sw, const ButtonState_t *btn) {
    int32_t tx;
    int32_t tz;
    int32_t pos;
    int32_t err;
    int32_t cmd_tgt;
    int8_t sign;

    tx = dds_get_position(AXIS_X);
    tz = dds_get_position(AXIS_Z);
    pos = (axis == AXIS_X) ? tx : tz;
    mpg_sync_overshoot(axis);
    pos = (axis == AXIS_X) ? dds_get_position(AXIS_X) : dds_get_position(AXIS_Z);
    err = mpg_cmd[axis] - pos;
    sign = (err > 0) ? 1 : ((err < 0) ? -1 : 0);
    cmd_tgt = mpg_cmd[axis];
    if (!dds_motion_jog_cruise_active()) {
        int32_t runway = mpg_runway(axis, sign, sw->mpg_scale, btn->joy_rapid);
        if (sign > 0 && err < runway) {
            cmd_tgt = pos + runway;
            if (cmd_tgt > mpg_cmd[axis]) cmd_tgt = mpg_cmd[axis];
        } else if (sign < 0 && (-err) < runway) {
            cmd_tgt = pos - runway;
            if (cmd_tgt < mpg_cmd[axis]) cmd_tgt = mpg_cmd[axis];
        }
    }
    if (mpg_dir_lock > 0 && cmd_tgt < pos) {
        cmd_tgt = pos;
    } else if (mpg_dir_lock < 0 && cmd_tgt > pos) {
        cmd_tgt = pos;
    }

    if (axis == AXIS_X) {
        tx = jog_clamp_target(AXIS_X, cmd_tgt, btn->joy_rapid);
    } else {
        tz = jog_clamp_target(AXIS_Z, cmd_tgt, btn->joy_rapid);
    }

    if (tx == dds_get_target(AXIS_X) && tz == dds_get_target(AXIS_Z)) {
        if (dds_get_position(axis) != mpg_cmd[axis]) {
            if (axis == AXIS_X) {
                tx = jog_clamp_target(AXIS_X, mpg_cmd[axis], btn->joy_rapid);
            } else {
                tz = jog_clamp_target(AXIS_Z, mpg_cmd[axis], btn->joy_rapid);
            }
        }
        if (tx == dds_get_target(AXIS_X) && tz == dds_get_target(AXIS_Z)) {
            DBG_VERBOSE("JOG", "MPG", "lim");
            mpg_active = 1;
            return;
        }
    }

    planner_exec_jog(tx, tz, jog_speed_mm_min(axis, btn->joy_rapid), "MPG", 1);
    mpg_active = 1;
}

void motion_jog_init(void) {
    hand_pos[AXIS_X] = 0;
    hand_pos[AXIS_Z] = 0;
    joy_z_on = 0;
    joy_x_on = 0;
    joy_tick_ms = 0;
    go_lim_active = 0;
    go_lim_latch = 0;
    go_lim_joy_arm = 0;
    mpg_active = 0;
    mpg_last_ms = 0;
    mpg_startup_sync = 1;
    mpg_axis_last = AXIS_Z;
    mpg_rev_cnt = 0;
    mpg_dir_lock = 0;
    mpg_batch_ms = 0;
    mpg_sync_cmd();
    motion_zero_all();
}

int32_t motion_jog_get_pos(uint8_t axis) {
    return motion_get_pos_steps(axis);
}

int32_t motion_jog_get_hand(uint8_t axis) {
    if (axis > AXIS_Z) return 0;
    return hand_pos[axis];
}

void motion_jog_reset_pos(uint8_t axis) {
    if (axis > AXIS_Z) return;
    hand_pos[axis] = 0;
}

void motion_jog_zero_axis(uint8_t axis) {
    if (axis > AXIS_Z) return;
    int32_t cur = motion_get_pos_steps(axis);
    if (go_lim_active && go_lim_axis == axis) {
        go_lim_active = 0;
        go_lim_latch = 0;
        go_lim_joy_arm = 0;
    }
    limits_rebase_axis(axis, cur);
    motion_set_pos_steps(axis, 0);
    hand_pos[axis] = 0;
    ui_encoder_reset_mpg();
    backlash_sync_axis(axis, (axis == AXIS_X) ? BACKLASH_REF_DIR_X : BACKLASH_REF_DIR_Z);
}

void motion_jog_go_limit(uint8_t idx) {
    uint8_t axis;
    int32_t target;

    if (estop_is_triggered()) return;
    if (!limits_ui_go_target(idx, &axis, &target)) return;
    if (dds_get_position(axis) == target) return;

    go_lim_axis = axis;
    go_lim_target = target;
    go_lim_active = 1;
    go_lim_latch = 0;
    go_lim_joy_arm = 1;

    planner_exec_axis(axis, target, jog_speed_mm_min(axis, 1), 0);
    DBG_INFO_VAL_I32("JOG", "GOLIM", "tgt", target);
}

void motion_jog_go_limit_latch(uint8_t idx) {
    uint8_t axis;
    int32_t target;

    if (estop_is_triggered()) return;
    if (!limits_ui_go_target(idx, &axis, &target)) return;
    if (dds_get_position(axis) == target) return;

    go_lim_axis = axis;
    go_lim_target = target;
    go_lim_active = 1;
    go_lim_latch = 1;
    go_lim_joy_arm = 0;

    planner_exec_axis(axis, target, jog_speed_mm_min(axis, 0), 0);
    DBG_INFO_VAL_I32("JOG", "LATCH", "tgt", target);
}

void motion_jog_resume(void) {
    go_lim_active = 0;
    go_lim_latch = 0;
    go_lim_joy_arm = 0;
    joy_z_on = 0;
    joy_x_on = 0;
    joy_tick_ms = 0;
    mpg_active = 0;
    mpg_last_ms = 0;
    mpg_rev_cnt = 0;
    mpg_dir_lock = 0;
    mpg_batch_ms = 0;
    mpg_sync_cmd();
    dds_motion_stop();
    dds_set_target(AXIS_X, dds_get_position(AXIS_X));
    dds_set_target(AXIS_Z, dds_get_position(AXIS_Z));
    (void)ui_encoder_get_mpg_delta();
}

static void motion_jog_limit_poll(void) {
    if (!go_lim_active) return;

    if (go_lim_latch) {
        if (!go_lim_joy_arm) {
            if (!ui_buttons_feed_joy_on()) go_lim_joy_arm = 1;
        } else if (ui_buttons_feed_joy_on()) {
            go_limit_stop();
            go_lim_stop_joy = 1;
            DBG_INFO("JOG", "LATCH", "stop");
            return;
        }
    } else if (ui_buttons_feed_joy_click() || ui_buttons_feed_joy_on()) {
        go_limit_stop();
        go_lim_stop_joy = 1;
        DBG_INFO("JOG", "GOLIM", "stop");
        return;
    }

    if (!dds_motion_busy()) {
        int32_t pos = dds_get_position(go_lim_axis);
        int32_t err = go_lim_target - pos;
        if (err != 0) {
            int32_t ad = (err < 0) ? -err : err;
            int32_t bl = backlash_get_steps(go_lim_axis);
            if (bl > 0 && ad <= bl) {
                dds_set_position(go_lim_axis, go_lim_target);
                backlash_sync_axis(go_lim_axis,
                    (go_lim_axis == AXIS_X) ? BACKLASH_REF_DIR_X : BACKLASH_REF_DIR_Z);
            }
        }
    }

    if (dds_at_target(go_lim_axis) && !dds_motion_busy()) {
        go_limit_stop();
        DBG_INFO("JOG", "GOLIM", "done");
        return;
    }

    if (dds_motion_busy()) {
        dds_motion_update_target(go_lim_axis, go_lim_target);
    } else {
        planner_exec_axis(go_lim_axis, go_lim_target,
                          jog_speed_mm_min(go_lim_axis, go_lim_latch ? 0 : 1), 0);
    }
}

void motion_jog_joy_poll(void) {
    int32_t tx;
    int32_t tz;

    if (estop_is_triggered()) {
        go_lim_active = 0;
        go_lim_latch = 0;
        go_lim_joy_arm = 0;
        return;
    }

    motion_jog_limit_poll();
    if (go_lim_active) return;
    if (go_lim_stop_joy) {
        go_lim_stop_joy = 0;
        return;
    }

    ButtonState_t btn = ui_buttons_get_state();

    int8_t z_sign = 0;
    if (btn.joy_left) z_sign = -1;
    else if (btn.joy_right) z_sign = 1;

    int8_t x_sign = 0;
    if (btn.joy_up) x_sign = 1;
    else if (btn.joy_down) x_sign = -1;

    uint8_t z_on = (z_sign != 0);
    uint8_t x_on = (x_sign != 0);
    uint8_t was_z_on = joy_z_on;
    uint8_t was_x_on = joy_x_on;

    if (z_on && !was_z_on) hand_reset_axis(AXIS_Z);
    if (x_on && !was_x_on) hand_reset_axis(AXIS_X);

    if ((z_on && !was_z_on) || (x_on && !was_x_on)) {
        if (dds_motion_busy()) {
            dds_motion_stop();
        }
    }

    joy_z_on = z_on;
    joy_x_on = x_on;

    if (!z_on && !x_on) {
        if ((was_z_on || was_x_on) && !mpg_active) {
            planner_jog_stop();
        }
        return;
    }

    unsigned long now = millis();
    if (now - joy_tick_ms < JOY_STEP_MS) return;
    joy_tick_ms = now;

    tx = dds_get_position(AXIS_X);
    tz = dds_get_position(AXIS_Z);
    int32_t ox = tx;
    int32_t oz = tz;

    if (z_on) {
        int32_t chunk_z = joy_chunk(AXIS_Z, btn.joy_rapid) * (int32_t)JOY_LOOKAHEAD;
        tz = jog_clamp_target(AXIS_Z, tz + z_sign * chunk_z, btn.joy_rapid);
    }
    if (x_on) {
        int32_t chunk_x = joy_chunk(AXIS_X, btn.joy_rapid) * (int32_t)JOY_LOOKAHEAD;
        tx = jog_clamp_target(AXIS_X, tx + x_sign * chunk_x, btn.joy_rapid);
    }

    if (tx == ox && tz == oz) {
        if (dds_motion_jog_cruise_active() && !mpg_active) {
            planner_jog_stop();
        }
        return;
    }

    planner_exec_jog(tx, tz, jog_speed_dual_mm_min(btn.joy_rapid), "JOY", 1);
}

void motion_jog_poll(void) {
    SwitchState_t sw;
    ButtonState_t btn;
    uint8_t axis;
    int32_t steps;
    int32_t total_steps = 0;
    int32_t tx;
    int32_t tz;

    if (estop_is_triggered()) {
        mpg_active = 0;
        mpg_rev_cnt = 0;
        mpg_dir_lock = 0;
        mpg_batch_ms = 0;
        return;
    }
    if (go_lim_active) return;

    if (mpg_startup_sync) {
        if (planner_startup_busy()) return;
        mpg_startup_sync = 0;
        ui_encoder_reset_mpg();
        hand_pos[AXIS_X] = 0;
        hand_pos[AXIS_Z] = 0;
        mpg_active = 0;
        mpg_rev_cnt = 0;
        mpg_dir_lock = 0;
        mpg_sync_cmd();
        dds_set_target(AXIS_X, dds_get_position(AXIS_X));
        dds_set_target(AXIS_Z, dds_get_position(AXIS_Z));
        backlash_sync_axis(AXIS_X, BACKLASH_REF_DIR_X);
        backlash_sync_axis(AXIS_Z, BACKLASH_REF_DIR_Z);
    }

    if (ui_encoder_peek_mpg_delta() == 0) {
        if (mpg_batch_ms != 0UL && (millis() - mpg_batch_ms) >= MPG_BATCH_MS) {
            sw = ui_switches_get_state();
            btn = ui_buttons_get_state();
            mpg_batch_ms = 0UL;
            mpg_planner_commit(mpg_axis_last, &sw, &btn);
            mpg_last_ms = millis();
            return;
        }
        if (mpg_active && (millis() - mpg_last_ms >= MPG_IDLE_STOP_MS)) {
            int32_t pos = dds_get_position(mpg_axis_last);
            int32_t cmd;

            mpg_sync_overshoot(mpg_axis_last);
            cmd = jog_clamp_target(mpg_axis_last, mpg_cmd[mpg_axis_last], 0);
            pos = dds_get_position(mpg_axis_last);

            if (pos != cmd) {
                tx = dds_get_position(AXIS_X);
                tz = dds_get_position(AXIS_Z);
                if (mpg_axis_last == AXIS_X) {
                    tx = cmd;
                } else {
                    tz = cmd;
                }
                planner_exec_jog(tx, tz, jog_speed_mm_min(mpg_axis_last, 0), "MPG", 1);
            } else if (dds_motion_busy() && dds_get_target(mpg_axis_last) != cmd) {
                tx = dds_get_position(AXIS_X);
                tz = dds_get_position(AXIS_Z);
                if (mpg_axis_last == AXIS_X) {
                    tx = cmd;
                } else {
                    tz = cmd;
                }
                planner_exec_jog(tx, tz, jog_speed_mm_min(mpg_axis_last, 0), "MPG", 1);
            } else {
                if (millis() - mpg_last_ms >= MPG_IDLE_DECEL_MS) {
                    mpg_active = 0;
                    mpg_rev_cnt = 0;
                    mpg_dir_lock = 0;
                    mpg_batch_ms = 0;
                }
            }
        }
        return;
    }

    sw = ui_switches_get_state();
    btn = ui_buttons_get_state();
    axis = sw.mpg_axis;
    if (axis != mpg_axis_last) {
        mpg_dir_lock = 0;
        mpg_rev_cnt = 0;
        mpg_batch_ms = 0;
    }
    mpg_axis_last = axis;

    if ((axis == AXIS_Z && joy_z_on) || (axis == AXIS_X && joy_x_on)) {
        DBG_VERBOSE("JOG", "MPG", "blk joy");
        return;
    }

    {
        int8_t lock_sign = 0;

        if (mpg_active && dds_motion_busy()) {
            lock_sign = mpg_motion_sign(axis);
            if (lock_sign != 0) mpg_dir_lock = lock_sign;
        } else if (mpg_dir_lock != 0) {
            lock_sign = mpg_dir_lock;
        }

        for (uint8_t n = 0; n < MPG_MAX_TICKS; n++) {
            int8_t tick_sign;
            int32_t peek_d;

            peek_d = ui_encoder_peek_mpg_delta();
            if (peek_d == 0) break;
            tick_sign = (peek_d > 0) ? 1 : -1;

            if (lock_sign == 0) {
                lock_sign = tick_sign;
                mpg_dir_lock = tick_sign;
            } else if (tick_sign != lock_sign) {
                (void)ui_encoder_consume_mpg_tick();
                mpg_rev_cnt++;
                if (mpg_rev_cnt <= MPG_REV_IGNORE_TICKS) {
                    DBG_VERBOSE("JOG", "MPG", "rev ign");
                    continue;
                }
                if (mpg_active && dds_motion_busy()) {
                    mpg_motion_abort(axis);
                }
                lock_sign = tick_sign;
                mpg_dir_lock = tick_sign;
                mpg_rev_cnt = 0;
                steps = jog_steps_from_delta(axis, tick_sign, sw.mpg_scale, btn.joy_rapid);
                if (steps != 0) total_steps += steps;
                continue;
            } else {
                mpg_rev_cnt = 0;
            }

            steps = jog_steps_from_delta(axis, tick_sign, sw.mpg_scale, btn.joy_rapid);
            (void)ui_encoder_consume_mpg_tick();
            if (steps != 0) total_steps += steps;
        }
    }

    if (total_steps == 0) return;

    mpg_last_ms = millis();
    if (mpg_batch_ms == 0UL) {
        mpg_batch_ms = mpg_last_ms;
    }

    {
        int32_t prev_cmd = mpg_cmd[axis];
        mpg_cmd[axis] = jog_clamp_target(axis, mpg_cmd[axis] + total_steps, btn.joy_rapid);
        hand_pos[axis] += mpg_cmd[axis] - prev_cmd;
    }

    if (ui_encoder_peek_mpg_delta() != 0 &&
        (millis() - mpg_batch_ms) < MPG_BATCH_MS) {
        mpg_active = 1;
        return;
    }
    mpg_batch_ms = 0UL;

    mpg_planner_commit(axis, &sw, &btn);
    DBG_VERBOSE_VAL_I32("JOG", "MPG", "s", total_steps);
}
