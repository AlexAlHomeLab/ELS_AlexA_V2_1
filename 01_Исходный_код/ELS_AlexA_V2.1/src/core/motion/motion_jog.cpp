#include "motion_jog.h"
#include "motion_control.h"
#include "planner.h"
#include "stepper_gen.h"
#include "backlash.h"
#include "../../config/config.h"
#include "../../config/config_machine.h"
#include "../debug/debug_serial.h"
#include "../debug/debug_trace.h"
#include "../process/estop_control.h"
#include "../motion/limits.h"
#include "../ui/ui_buttons.h"
#include "../ui/ui_encoder.h"
#include "../ui/ui_pot.h"
#include "../ui/ui_switches.h"
#include "../fsm/fsm_manager.h"
#include "../../config/config_feed.h"

#include <Arduino.h>

#define JOY_STEP_MS 20UL          /* период тика джойстика */
#define JOY_LOOKAHEAD 8           /* множитель chunk при джоге */
#define MPG_MAX_TICKS 24          /* макс. тиков РГИ за один poll */
#define MPG_LOOKAHEAD 4           /* runway шагов вперёд для cruise */
#define MPG_MAX_RUNWAY_STEPS 512  /* верхняя граница runway */
#define MPG_MAX_CMD_AHEAD 4096    /* макс. опережение cmd над position, шаги */
#define MPG_IDLE_STOP_MS 80UL     /* пауза после тика → дотягивание до cmd */
#define MPG_BATCH_MS 8UL          /* ожидание всех detent перед первым exec */
#define MPG_IDLE_DECEL_MS 250UL   /* сброс mpg_active после простоя */
#define MPG_REV_IGNORE_TICKS 5    /* игнор тиков при смене направления РГИ */
#define MPG_AXIS_ARM_LOOPS 2U     /* пропуск poll после смены оси РГИ */

static int32_t hand_pos[2];       /* накопленное смещение РГИ, шаги */
static uint8_t joy_z_on;          /* джойстик Z активен */
static uint8_t joy_x_on;          /* джойстик X активен */
static unsigned long joy_tick_ms;
static uint8_t go_lim_active;     /* движение к программному лимиту */
static uint8_t go_lim_axis;
static int32_t go_lim_target;
static uint8_t go_lim_stop_joy;   /* флаг: остановили из-за джойстика */
static uint8_t go_lim_latch;      /* 1 — latch-режим (стоп при джойстике) */
static uint8_t go_lim_joy_arm;
static uint8_t mpg_active;        /* сессия РГИ активна */
static unsigned long mpg_last_ms;
static uint8_t mpg_startup_sync = 1;  /* ждём planner_startup_busy */
static int32_t mpg_cmd[2];        /* командная позиция РГИ (не dds position) */
static uint8_t mpg_axis_last;     /* последняя ось селектора РГИ */
static uint8_t mpg_rev_cnt;       /* счётчик тиков реверса */
static int8_t mpg_dir_lock;       /* заблокированное направление (+1/-1) */
static unsigned long mpg_batch_ms;
static uint8_t mpg_axis_arm;      /* >0 — пропуск poll после смены оси */
static SwitchState_t mpg_sw;
static ButtonState_t mpg_btn;

static void go_limit_stop(void) {  /* отмена go_lim */
    if (!go_lim_active) return;
    go_lim_active = 0;
    go_lim_latch = 0;
    go_lim_joy_arm = 0;
    dds_motion_stop();
    dds_set_target(go_lim_axis, dds_get_position(go_lim_axis));
}

static void hand_reset_axis(uint8_t axis) {
    hand_pos[axis] = 0;
    mpg_cmd[axis] = dds_get_position(axis);
    if (axis == mpg_axis_last) {
        mpg_dir_lock = 0;
        mpg_rev_cnt = 0;
    }
    ui_encoder_reset_mpg();
    DBG_VERBOSE_VAL("JOG", "HAND", axis == AXIS_X ? "Xrst" : "Zrst", 0);
}

/* Шаги за один тик РГИ: scale 1 = 0.01 мм, rapid = 0.1 мм */
static int8_t mpg_adjust_tick_sign(uint8_t axis, int8_t tick_sign) {
    if (tick_sign == 0) return 0;
    /* X: компенсация dir_invert — РГИ X без изменения направления */
    if (axis == AXIS_X && config_get_dir_invert(AXIS_X)) {
        tick_sign = (int8_t)(-tick_sign);
    }
#if MPG_AXIS_Z_INVERT
    if (axis == AXIS_Z) {
        tick_sign = (int8_t)(-tick_sign);
    }
#endif
    return tick_sign;
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

static float jog_speed_mm_min(uint8_t axis, uint8_t rapid) {  /* pot или rapid_speed */
    float mm_min;
    FeedUnit_t unit;

    if (rapid) {
        mm_min = (float)config_get_rapid_speed_mm_min(axis);
    } else {
        mm_min = ui_pot_get_jog_mm_min();
        unit = config_feed_get_unit(fsm_manager_get_mode());
        /* sync: мм/об × rpm даёт малые мм/мин — не поднимать до 10 как в async */
        if (unit == FEED_UNIT_MM_REV) {
            if (mm_min < 1.0f) {
                mm_min = 1.0f;
            }
        } else if (mm_min < 10.0f) {
            mm_min = 10.0f;
        }
        {
            float cap = (float)config_get_max_speed_mm_min(axis);
            if (mm_min > cap) {
                mm_min = cap;
            }
        }
    }
    return mm_min;
}

static float jog_speed_dual_mm_min(uint8_t rapid) {
    float sx = jog_speed_mm_min(AXIS_X, rapid);
    float sz = jog_speed_mm_min(AXIS_Z, rapid);
    return (sx > sz) ? sx : sz;
}

static int32_t joy_chunk(uint8_t axis, uint8_t rapid) {  /* шагов за JOY_STEP_MS */
    float mm_min = jog_speed_mm_min(axis, rapid);
    float spm = (axis == AXIS_X) ? STEPS_PER_MM_X : STEPS_PER_MM_Z;
    int32_t chunk = (int32_t)((mm_min * spm) / (60.0f * (1000.0f / (float)JOY_STEP_MS)));
    if (chunk < 1) chunk = 1;
    return chunk;
}

static int32_t jog_clamp_target(uint8_t axis, int32_t target, uint8_t rapid) {
    /* rapid — без clamp лимитов */
    if (rapid) return target;
    return limits_clamp_steps(axis, target);
}

/* Один тик РГИ → mpg_cmd; лог координаты назначения */
static void mpg_apply_tick(uint8_t axis, int32_t steps, uint8_t rapid, uint8_t *lim_hit) {
    int32_t prev;
    int32_t raw;

    if (steps == 0) return;

    prev = mpg_cmd[axis];
    raw = mpg_cmd[axis] + steps;
    mpg_cmd[axis] = jog_clamp_target(axis, raw, rapid);
    if (lim_hit != NULL && !rapid && mpg_cmd[axis] != raw) {
        *lim_hit = 1U;
    }
    hand_pos[axis] += mpg_cmd[axis] - prev;
}

static void mpg_sync_cmd(void) {  /* mpg_cmd = текущая позиция DDS */
    mpg_cmd[AXIS_X] = dds_get_position(AXIS_X);
    mpg_cmd[AXIS_Z] = dds_get_position(AXIS_Z);
}

static int32_t mpg_runway(uint8_t axis, int8_t sign, uint8_t mpg_scale, uint8_t rapid) {
    /* запас шагов вперёд для jog cruise */
    int32_t step;
    int32_t rw;

    if (sign == 0) return 0;
    step = jog_steps_from_delta(axis, sign, mpg_scale, rapid);
    if (step < 0) step = -step;
    rw = step * (int32_t)MPG_LOOKAHEAD;
    if (rw > MPG_MAX_RUNWAY_STEPS) rw = MPG_MAX_RUNWAY_STEPS;
    return rw;
}

static int32_t mpg_clamp_cmd_ahead(int32_t pos, int32_t cmd_tgt) {
    int32_t d = cmd_tgt - pos;

    if (d > MPG_MAX_CMD_AHEAD) return pos + MPG_MAX_CMD_AHEAD;
    if (d < -MPG_MAX_CMD_AHEAD) return pos - MPG_MAX_CMD_AHEAD;
    return cmd_tgt;
}

static int8_t mpg_motion_sign(uint8_t axis) {  /* знак ошибки mpg_cmd − position */
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

static void mpg_session_halt(void) {  /* полный стоп MPG/jog + sync cmd */
    planner_jog_halt();
    mpg_active = 0;
    mpg_dir_lock = 0;
    mpg_rev_cnt = 0;
    mpg_batch_ms = 0;
    mpg_sync_cmd();
}

static void mpg_motion_abort(uint8_t axis) {  /* стоп при реверсе РГИ на ходу */
    (void)axis;
    mpg_session_halt();
}

void motion_jog_on_axis_select(uint8_t new_axis) {
    if (new_axis > AXIS_Z) return;
    if (new_axis == mpg_axis_last) return;
    if (planner_startup_busy()) {
        mpg_axis_last = new_axis;
        ui_encoder_discard_mpg_delta();
        mpg_axis_arm = MPG_AXIS_ARM_LOOPS;
        return;
    }
    mpg_session_halt();
    ui_encoder_discard_mpg_delta();
    mpg_axis_last = new_axis;
    mpg_axis_arm = MPG_AXIS_ARM_LOOPS;
}

static void mpg_sync_overshoot(uint8_t axis) {  /* cmd не отстаёт от position при lock */
    int32_t pos = dds_get_position(axis);

    if (mpg_dir_lock > 0 && pos > mpg_cmd[axis]) {
        mpg_cmd[axis] = pos;
    } else if (mpg_dir_lock < 0 && pos < mpg_cmd[axis]) {
        mpg_cmd[axis] = pos;
    }
}

static void mpg_planner_commit(uint8_t axis, const SwitchState_t *sw, const ButtonState_t *btn,
                               uint8_t lim_hit) {
    TRACE_ENTER(TR_MPG_COMMIT);
    /* mpg_cmd → planner_exec_jog cruise */
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
    cmd_tgt = mpg_clamp_cmd_ahead(pos, cmd_tgt);

    if (axis == AXIS_X) {
        tx = jog_clamp_target(AXIS_X, cmd_tgt, btn->joy_rapid);
        if (!lim_hit && !btn->joy_rapid && tx != cmd_tgt) lim_hit = 1U;
    } else {
        tz = jog_clamp_target(AXIS_Z, cmd_tgt, btn->joy_rapid);
        if (!lim_hit && !btn->joy_rapid && tz != cmd_tgt) lim_hit = 1U;
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
            DBG_JOG_MOVE_LIM("MPG", tx, tz, jog_speed_mm_min(axis, btn->joy_rapid), 0, 1U, 0U, 0L);
            mpg_active = 1;
            return;
        }
    }

    if (planner_exec_jog(tx, tz, jog_speed_mm_min(axis, btn->joy_rapid), "MPG", 1, lim_hit, 0U, 0L)) {
        mpg_active = 1;
        DBG_MPG_PULSE(axis, mpg_cmd[axis]);
    }
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
    mpg_axis_arm = 0;
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
    DBG_INFO("JOG", "JOY", "resume");
}

static void motion_jog_limit_poll(void) {  /* сопровождение go_lim до цели */
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
                DBG_LIM_CMP(go_lim_axis, ad);
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

void motion_jog_joy_poll(void) {  /* джойстик: chunk + lookahead, cruise jog */
    int32_t tx;
    int32_t tz;
    static uint8_t joy_run = 0;

    if (estop_is_triggered()) {
        if (joy_run) {
            DBG_INFO("JOG", "JOY", "blk estop");
            joy_run = 0;
        }
        go_lim_active = 0;
        go_lim_latch = 0;
        go_lim_joy_arm = 0;
        return;
    }

    motion_jog_limit_poll();
    if (go_lim_active) {
        if (joy_run) {
            DBG_INFO("JOG", "JOY", "blk golim");
            joy_run = 0;
        }
        return;
    }
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
    uint8_t joy_any = (uint8_t)(z_on | x_on);

    if (joy_any && !joy_run) {
        DBG_INFO_VAL("UI", "JOY", "run", (uint32_t)((z_sign + 2) | ((x_sign + 2) << 2)));
        joy_run = 1;
    } else if (!joy_any && joy_run) {
        DBG_INFO("JOG", "JOY", "idle");
        joy_run = 0;
    }

    if (z_on && !was_z_on) hand_reset_axis(AXIS_Z);
    if (x_on && !was_x_on) hand_reset_axis(AXIS_X);

    if ((z_on && !was_z_on) || (x_on && !was_x_on)) {
        if (dds_motion_busy() || backlash_pending(AXIS_X) > 0 || backlash_pending(AXIS_Z) > 0) {
            planner_jog_stop();
        }
    }

    joy_z_on = z_on;
    joy_x_on = x_on;

    if (!z_on && !x_on) {
        if (was_z_on) {
            mpg_cmd[AXIS_Z] = dds_get_position(AXIS_Z);
            if (mpg_axis_last == AXIS_Z) {
                mpg_dir_lock = 0;
                mpg_rev_cnt = 0;
            }
        }
        if (was_x_on) {
            mpg_cmd[AXIS_X] = dds_get_position(AXIS_X);
            if (mpg_axis_last == AXIS_X) {
                mpg_dir_lock = 0;
                mpg_rev_cnt = 0;
            }
        }
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

    /* Лимит/цель=позиция: cruise не гасим — только обновить скорость с пота (7e2 aFeed) */
    if (tx == ox && tz == oz) {
        if (dds_motion_jog_cruise_active() && !mpg_active) {
            planner_exec_jog(tx, tz, jog_speed_dual_mm_min(btn.joy_rapid), "JOY", 1, 0U, 0U, 0L);
        }
        return;
    }

    planner_exec_jog(tx, tz, jog_speed_dual_mm_min(btn.joy_rapid), "JOY", 1, 0U, 0U, 0L);
}

void motion_jog_poll(void) {  /* РГИ: batch тиков, dir_lock, idle stop */
    uint8_t axis;
    int32_t steps;
    uint8_t any_tick = 0U;
    int32_t tx;
    int32_t tz;
    uint8_t lim_hit = 0U;
    SwitchState_t *sw = &mpg_sw;
    ButtonState_t *btn = &mpg_btn;

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
        {
            SwitchState_t sw0 = ui_switches_get_state();
            mpg_axis_last = sw0.mpg_axis;
        }
    }

    {
        SwitchState_t sw0 = ui_switches_get_state();
        motion_jog_on_axis_select(sw0.mpg_axis);
    }

    if (mpg_axis_arm > 0U) {
        ui_encoder_discard_mpg_delta();
        mpg_axis_arm--;
        return;
    }

    if (ui_encoder_peek_mpg_delta() == 0) {
        if (mpg_batch_ms != 0UL && (millis() - mpg_batch_ms) >= MPG_BATCH_MS) {
            *sw = ui_switches_get_state();
            *btn = ui_buttons_get_state();
            mpg_batch_ms = 0UL;
            mpg_planner_commit(mpg_axis_last, sw, btn, 0U);
            mpg_last_ms = millis();
            return;
        }
        if (mpg_active && (millis() - mpg_last_ms >= MPG_IDLE_STOP_MS)) {
            int32_t pos = dds_get_position(mpg_axis_last);
            int32_t cmd;

            mpg_sync_overshoot(mpg_axis_last);
            {
                int32_t raw_cmd = mpg_cmd[mpg_axis_last];
                uint8_t lim_hit = 0U;

                cmd = jog_clamp_target(mpg_axis_last, raw_cmd, 0);
                if (cmd != raw_cmd) lim_hit = 1U;
                pos = dds_get_position(mpg_axis_last);

                if (pos != cmd) {
                    tx = dds_get_position(AXIS_X);
                    tz = dds_get_position(AXIS_Z);
                    if (mpg_axis_last == AXIS_X) {
                        tx = cmd;
                    } else {
                        tz = cmd;
                    }
                    planner_exec_jog(tx, tz, jog_speed_mm_min(mpg_axis_last, 0), "MPG", 1,
                                     lim_hit, 0U, 0L);
                } else if (dds_motion_busy() && dds_get_target(mpg_axis_last) != cmd) {
                    tx = dds_get_position(AXIS_X);
                    tz = dds_get_position(AXIS_Z);
                    if (mpg_axis_last == AXIS_X) {
                        tx = cmd;
                    } else {
                        tz = cmd;
                    }
                    planner_exec_jog(tx, tz, jog_speed_mm_min(mpg_axis_last, 0), "MPG", 1,
                                     lim_hit, 0U, 0L);
                } else {
                    if (millis() - mpg_last_ms >= MPG_IDLE_DECEL_MS) {
                        if (dds_motion_jog_cruise_active()) {
                            planner_jog_stop();
                        }
                        mpg_active = 0;
                        mpg_rev_cnt = 0;
                        mpg_dir_lock = 0;
                        mpg_batch_ms = 0;
                    }
                }
            }
        }
        return;
    }

    *sw = ui_switches_get_state();
    *btn = ui_buttons_get_state();
    axis = sw->mpg_axis;

    if ((axis == AXIS_Z && joy_z_on) || (axis == AXIS_X && joy_x_on)) {
        DBG_VERBOSE("JOG", "MPG", "blk joy");
        return;
    }

    {
        int8_t lock_sign = 0;

        if (mpg_active && dds_motion_busy()) {
            lock_sign = mpg_motion_sign(axis);
            if (lock_sign != 0) {
                mpg_dir_lock = lock_sign;
            } else if (mpg_dir_lock != 0) {
                /* pos==target, cruise ещё active — знак из dir_lock, иначе реверс без фильтра */
                lock_sign = mpg_dir_lock;
            }
        } else if (mpg_dir_lock != 0) {
            lock_sign = mpg_dir_lock;
        }

        for (uint8_t n = 0; n < MPG_MAX_TICKS; n++) {
            int8_t tick_sign;
            int32_t peek_d;

            peek_d = ui_encoder_peek_mpg_delta();
            if (peek_d == 0) break;
            tick_sign = (peek_d > 0) ? 1 : -1;
            tick_sign = mpg_adjust_tick_sign(axis, tick_sign);

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
                steps = jog_steps_from_delta(axis, tick_sign, sw->mpg_scale, btn->joy_rapid);
                mpg_apply_tick(axis, steps, btn->joy_rapid, &lim_hit);
                if (steps != 0) any_tick = 1U;
                continue;
            } else {
                mpg_rev_cnt = 0;
            }

            steps = jog_steps_from_delta(axis, tick_sign, sw->mpg_scale, btn->joy_rapid);
            (void)ui_encoder_consume_mpg_tick();
            mpg_apply_tick(axis, steps, btn->joy_rapid, &lim_hit);
            if (steps != 0) any_tick = 1U;
        }
    }

    if (!any_tick) return;

    mpg_last_ms = millis();
    if (mpg_batch_ms == 0UL) {
        mpg_batch_ms = mpg_last_ms;
    }

    if (ui_encoder_peek_mpg_delta() != 0 &&
        (millis() - mpg_batch_ms) < MPG_BATCH_MS) {
        mpg_active = 1;
        return;
    }
    mpg_batch_ms = 0UL;

    mpg_planner_commit(axis, sw, btn, lim_hit);
}
