#include "motion_jog.h"
#include "stepper_gen.h"
#include "../../config/config.h"
#include "../../config/config_storage.h"
#include "../debug/debug_serial.h"
#include "../process/estop_control.h"
#include "../ui/ui_buttons.h"
#include "../ui/ui_encoder.h"
#include "../ui/ui_pot.h"
#include "../ui/ui_switches.h"

#include <Arduino.h>

#define JOY_STEP_MS 20UL

/* hand_pos — накопитель РГИ (сброс не меняет координаты); координаты = DDS */
static int32_t hand_pos[2];
static uint8_t joy_z_on;
static uint8_t joy_x_on;
static unsigned long joy_tick_ms;

static int32_t axis_cur_pos(uint8_t axis) {
    if (dds_at_target(axis)) {
        return dds_get_position(axis);
    }
    return dds_get_target(axis);
}

static void hand_reset_axis(uint8_t axis) {
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

static uint32_t jog_speed_sps(uint8_t axis) {
    float steps_per_mm = (axis == AXIS_X) ? STEPS_PER_MM_X : STEPS_PER_MM_Z;
    uint32_t feed_pct = ui_pot_get_percent();
    uint32_t feed_max = config_get_feed_max();
    uint32_t mm_min = (feed_pct * feed_max) / 100U;
    if (mm_min < 10) mm_min = 10;

    uint32_t sps = (uint32_t)((mm_min * steps_per_mm) / 60.0f);
    if (sps < JOG_SPEED_MIN_SPS) sps = JOG_SPEED_MIN_SPS;
    if (sps > JOG_SPEED_MAX_SPS) sps = JOG_SPEED_MAX_SPS;
    return sps;
}

static int32_t joy_chunk(uint8_t axis, uint8_t rapid) {
    uint32_t sps = jog_speed_sps(axis);
    if (rapid) {
        sps *= 10U;
        if (sps > JOG_SPEED_MAX_SPS * 10U) sps = JOG_SPEED_MAX_SPS * 10U;
    }
    int32_t chunk = (int32_t)((sps * JOY_STEP_MS) / 1000UL);
    if (chunk < 1) chunk = 1;
    return chunk;
}

void motion_jog_init(void) {
    hand_pos[AXIS_X] = 0;
    hand_pos[AXIS_Z] = 0;
    joy_z_on = 0;
    joy_x_on = 0;
    joy_tick_ms = 0;
    dds_set_position(AXIS_X, 0);
    dds_set_position(AXIS_Z, 0);
    dds_set_target(AXIS_X, 0);
    dds_set_target(AXIS_Z, 0);
}

int32_t motion_jog_get_pos(uint8_t axis) {
    if (axis > AXIS_Z) return 0;
    return axis_cur_pos(axis);
}

int32_t motion_jog_get_hand(uint8_t axis) {
    if (axis > AXIS_Z) return 0;
    return hand_pos[axis];
}

void motion_jog_reset_pos(uint8_t axis) {
    if (axis > AXIS_Z) return;
    hand_pos[axis] = 0;
}

void motion_jog_joy_poll(void) {
    if (estop_is_triggered()) return;

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
        int32_t target = axis_cur_pos(AXIS_Z) + z_sign * chunk_z;
        dds_set_target(AXIS_Z, target);
        dds_enable(AXIS_Z, 1);
        dds_set_speed(AXIS_Z, jog_speed_sps(AXIS_Z));
    }
    if (x_on) {
        int32_t target = axis_cur_pos(AXIS_X) + x_sign * chunk_x;
        dds_set_target(AXIS_X, target);
        dds_enable(AXIS_X, 1);
        dds_set_speed(AXIS_X, jog_speed_sps(AXIS_X));
    }
}

void motion_jog_poll(void) {
    if (estop_is_triggered()) return;

    int32_t delta = ui_encoder_get_mpg_delta();
    if (delta == 0) return;

    SwitchState_t sw = ui_switches_get_state();
    ButtonState_t btn = ui_buttons_get_state();
    uint8_t axis = sw.mpg_axis;

    if ((axis == AXIS_Z && joy_z_on) || (axis == AXIS_X && joy_x_on)) return;

    int32_t steps = jog_steps_from_delta(axis, delta, sw.mpg_scale, btn.joy_rapid);
    if (steps == 0) return;

    hand_pos[axis] += steps;

    int32_t target = axis_cur_pos(axis) + steps;
    dds_set_target(axis, target);
    dds_enable(axis, 1);
    dds_set_speed(axis, jog_speed_sps(axis));
    DBG_JOG(steps, axis, target);
}
