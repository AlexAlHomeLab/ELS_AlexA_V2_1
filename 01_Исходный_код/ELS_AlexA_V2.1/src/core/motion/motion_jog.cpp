#include "motion_jog.h"
#include "motion_control.h"
#include "stepper_gen.h"
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

/* hand_pos — накопитель РГИ; сброс только при старте джойстика по оси */
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

static void go_limit_stop(void) {
    if (!go_lim_active) return;
    go_lim_active = 0;
    go_lim_latch = 0;
    go_lim_joy_arm = 0;
    int32_t pos = dds_get_position(go_lim_axis);
    dds_set_target(go_lim_axis, pos);
    dds_enable(go_lim_axis, 1);
}

static void hand_reset_axis(uint8_t axis) {
    /* только джойстик; смена масштаба/оси РГИ hand_pos не трогает */
    hand_pos[axis] = 0;
    ui_encoder_reset_mpg();
}

/* Масштаб РГИ: x1 = 1 шаг/щелчок, x0.01 = 0.01 мм/тик INT2, Rapid = 0.1 мм */
static int32_t jog_steps_from_delta(uint8_t axis, int32_t delta, uint8_t mpg_scale, uint8_t rapid) {
    if (delta == 0) return 0;

    int32_t spm = (int32_t)((axis == AXIS_X) ? STEPS_PER_MM_X : STEPS_PER_MM_Z);
    int32_t sign = (delta > 0) ? 1 : -1;

    if (rapid) {
        int32_t steps = delta * (spm / 10);
        if (steps == 0) steps = sign;
        return steps;
    }
    if (mpg_scale == 1) {
        int32_t steps = delta * (spm / 100);
        if (steps == 0) steps = sign;
        return steps;
    }
    return sign;
}

static uint32_t jog_speed_sps(uint8_t axis, uint8_t rapid) {
    float mm_min;

    if (rapid) {
        mm_min = (float)config_get_rapid_speed_mm_min(axis);
    } else {
        mm_min = ui_pot_get_jog_mm_min();
        if (mm_min < 10.0f) mm_min = 10.0f;
        float cap = (float)config_get_max_speed_mm_min(axis);
        if (mm_min > cap) mm_min = cap;
    }

    return config_mm_min_to_sps(axis, mm_min);
}

static int32_t joy_chunk(uint8_t axis, uint8_t rapid) {
    uint32_t sps = jog_speed_sps(axis, rapid);
    int32_t chunk = (int32_t)((sps * JOY_STEP_MS) / 1000UL);
    if (chunk < 1) chunk = 1;
    return chunk;
}

static int32_t jog_clamp_target(uint8_t axis, int32_t target, uint8_t rapid) {
    if (rapid) return target;
    return limits_clamp_steps(axis, target);
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
}

void motion_jog_go_limit(uint8_t idx) {
    uint8_t axis;
    int32_t target;

    if (estop_is_triggered()) return;
    if (!limits_ui_go_target(idx, &axis, &target)) return;
    if (motion_get_pos_steps(axis) == target) return;

    go_lim_axis = axis;
    go_lim_target = target;
    go_lim_active = 1;
    go_lim_latch = 0;
    go_lim_joy_arm = 1;

    dds_set_target(axis, target);
    dds_enable(axis, 1);
    dds_set_speed(axis, jog_speed_sps(axis, 1));
    DBG_INFO_VAL("JOG", "GOLIM", "tgt", (uint32_t)target);
}

void motion_jog_go_limit_latch(uint8_t idx) {
    uint8_t axis;
    int32_t target;

    if (estop_is_triggered()) return;
    if (!limits_ui_go_target(idx, &axis, &target)) return;
    if (motion_get_pos_steps(axis) == target) return;

    go_lim_axis = axis;
    go_lim_target = target;
    go_lim_active = 1;
    go_lim_latch = 1;
    go_lim_joy_arm = 0;

    dds_set_target(axis, target);
    dds_enable(axis, 1);
    dds_set_speed(axis, jog_speed_sps(axis, 0));
    DBG_INFO_VAL("JOG", "LATCH", "tgt", (uint32_t)target);
}

void motion_jog_resume(void) {
    go_lim_active = 0;
    go_lim_latch = 0;
    go_lim_joy_arm = 0;
    joy_z_on = 0;
    joy_x_on = 0;
    joy_tick_ms = 0;
    dds_set_target(AXIS_X, dds_get_position(AXIS_X));
    dds_set_target(AXIS_Z, dds_get_position(AXIS_Z));
    dds_enable(AXIS_X, 1);
    dds_enable(AXIS_Z, 1);
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

    if (dds_at_target(go_lim_axis)) {
        go_limit_stop();
        DBG_INFO("JOG", "GOLIM", "done");
        return;
    }

    dds_set_target(go_lim_axis, go_lim_target);
    dds_set_speed(go_lim_axis, jog_speed_sps(go_lim_axis, go_lim_latch ? 0 : 1));
    dds_enable(go_lim_axis, 1);
}

void motion_jog_joy_poll(void) {
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

    if (z_on && !joy_z_on) hand_reset_axis(AXIS_Z);
    if (x_on && !joy_x_on) hand_reset_axis(AXIS_X);

    joy_z_on = z_on;
    joy_x_on = x_on;

    if (!z_on && !x_on) return;

    unsigned long now = millis();
    if (now - joy_tick_ms < JOY_STEP_MS) return;
    joy_tick_ms = now;

    int32_t chunk_z = joy_chunk(AXIS_Z, btn.joy_rapid);
    int32_t chunk_x = joy_chunk(AXIS_X, btn.joy_rapid);

    if (z_on) {
        int32_t target = jog_clamp_target(AXIS_Z,
            motion_get_pos_steps(AXIS_Z) + z_sign * chunk_z, btn.joy_rapid);
        dds_set_target(AXIS_Z, target);
        dds_enable(AXIS_Z, 1);
        dds_set_speed(AXIS_Z, jog_speed_sps(AXIS_Z, btn.joy_rapid));
    }
    if (x_on) {
        int32_t target = jog_clamp_target(AXIS_X,
            motion_get_pos_steps(AXIS_X) + x_sign * chunk_x, btn.joy_rapid);
        dds_set_target(AXIS_X, target);
        dds_enable(AXIS_X, 1);
        dds_set_speed(AXIS_X, jog_speed_sps(AXIS_X, btn.joy_rapid));
    }
}

void motion_jog_poll(void) {
    if (estop_is_triggered()) return;
    if (go_lim_active) return;

    int32_t delta = ui_encoder_peek_mpg_delta();
    if (delta == 0) return;

    SwitchState_t sw = ui_switches_get_state();
    ButtonState_t btn = ui_buttons_get_state();
    uint8_t axis = sw.mpg_axis;

    if ((axis == AXIS_Z && joy_z_on) || (axis == AXIS_X && joy_x_on)) return;

    int32_t steps = jog_steps_from_delta(axis, delta, sw.mpg_scale, btn.joy_rapid);
    if (steps == 0) return;

    int32_t cur = motion_get_pos_steps(axis);
    int32_t target = jog_clamp_target(axis, cur + steps, btn.joy_rapid);
    int32_t actual = target - cur;
    if (actual == 0) return;

    (void)ui_encoder_get_mpg_delta();
    hand_pos[axis] += actual;

    dds_set_target(axis, target);
    dds_enable(axis, 1);
    dds_set_speed(axis, jog_speed_sps(axis, btn.joy_rapid));
    DBG_JOG(actual, axis, target);
}
