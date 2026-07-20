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
#include "../hal/hal_buzzer.h"
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
/* Retarget при остатке < порога; заполнение — почти PLN_MAX (реже cli) */
#define JOY_RUNWAY_REFRESH 8192L
#define JOY_RUNWAY_FILL    16000L
#define MPG_MAX_TICKS 24          /* макс. тиков РГИ за один poll */
#define MPG_REV_IGNORE_N 3        /* ТЗ: реверс ≤N игнор, >N — halt */
#define MPG_MAX_CMD_AHEAD 4096    /* макс. опережение цели над position, шаги */
#define MPG_MAX_LEAD 128          /* pad за mpg_cmd (не от pos — иначе runaway) */
#define MPG_IDLE_MS 2500UL        /* конец сессии: нет тиков ≥ IDLE */
#define MPG_COAST_MS 300UL        /* после тика: держим pad за cmd, без refresh от pos */
#define MPG_BATCH_MS 8UL          /* пакет детентов перед первым commit */
#define MPG_AXIS_ARM_LOOPS 2U     /* пропуск poll после смены оси РГИ */

static int32_t hand_pos[2];       /* накопленное смещение РГИ, шаги */
static uint8_t joy_z_on;          /* джойстик Z активен */
static uint8_t joy_x_on;          /* джойстик X активен */
static unsigned long joy_tick_ms;
static uint8_t go_lim_active;     /* движение к программному лимиту */
static uint8_t go_lim_axis;
static int32_t go_lim_target;
static uint8_t go_lim_stop_joy;   /* стоп go_lim джойстиком: ждём отпускания перед jog */
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
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
static uint8_t mpg_rapid_prev;    /* предыдущее состояние Rapid для фронтов */
static uint8_t mpg_approach_arm;  /* 1 — Rapid удерживается, движение отложено */
static uint8_t mpg_approach_ticks; /* 1 — за удержание Rapid были тики РГИ */
static uint8_t mpg_approach_go;   /* 1 — идёт подвод к mpg_cmd после отпускания Rapid */
#endif

/* 1 — Rapid откладывает planner commit (режим точного подвода) */
static uint8_t mpg_rapid_deferred(const ButtonState_t *btn) {
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
    return btn->joy_rapid;
#else
    (void)btn;
    return 0;
#endif
}

/* Обход лимитов при Rapid: только в LIVE-режиме */
static uint8_t mpg_lim_bypass(const ButtonState_t *btn) {
    if (!btn->joy_rapid) return 0U;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_LIVE
    return 1U;
#else
    return 0U;
#endif
}

/* 0.1 мм/тик от Rapid — только LIVE; APPROACH: масштаб с селектора */
static uint8_t mpg_step_use_rapid(const ButtonState_t *btn) {
#if MPG_RAPID_MODE == MPG_RAPID_MODE_LIVE
    return btn->joy_rapid;
#else
    (void)btn;
    return 0U;
#endif
}

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
        /* иначе idle MPG coast тянет к старому cmd против JOY */
        mpg_active = 0;
        mpg_batch_ms = 0;
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
    /* Диаметр X: половина хода по радиусу → на LCD тот же шаг 0.01 / 0.1 диаметра */
    uint8_t diam = (axis == AXIS_X &&
                    config_get_x_coord_mode() == X_COORD_MODE_DIAMETER) ? 1U : 0U;

    if (delta == 0) return 0;
    if (delta > 1) delta = 1;
    if (delta < -1) delta = -1;

    spm = (int32_t)((axis == AXIS_X) ? STEPS_PER_MM_X : STEPS_PER_MM_Z);

    if (rapid) {
        step = diam ? (spm / 20) : (spm / 10);  /* 0.05 мм радиуса = 0.1 диаметра */
        if (step < 1) step = 1;
        return delta > 0 ? step : -step;
    }
    if (mpg_scale == 1) {
        step = diam ? (spm / 200) : (spm / 100);  /* 0.005 мм радиуса = 0.01 диаметра */
        if (step < 1) step = 1;
        return delta > 0 ? step : -step;
    }
    return delta;  /* x1step — без учёта Xdia */
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

/* Скорость РГИ по режиму масштаба/Rapid; approach=1 — подвод APPROACH (отпускание Rapid) */
static float mpg_speed_mm_min(uint8_t axis, uint8_t mpg_scale, uint8_t rapid, uint8_t approach) {
    uint16_t spd;
    float mm_min;
    float cap;

    if (approach) {
        spd = (axis == AXIS_X) ? MPG_SPEED_APPROACH_X_MM_MIN : MPG_SPEED_APPROACH_Z_MM_MIN;
    } else if (rapid) {
        spd = (axis == AXIS_X) ? MPG_SPEED_01_LIVE_X_MM_MIN : MPG_SPEED_01_LIVE_Z_MM_MIN;
    } else if (mpg_scale == 1) {
        spd = (axis == AXIS_X) ? MPG_SPEED_001_X_MM_MIN : MPG_SPEED_001_Z_MM_MIN;
    } else {
        spd = (axis == AXIS_X) ? MPG_SPEED_X1_X_MM_MIN : MPG_SPEED_X1_Z_MM_MIN;
    }
    mm_min = (float)spd;
    if (mm_min < 1.0f) {
        mm_min = 1.0f;
    }
    cap = (float)config_get_max_speed_mm_min(axis);
    if (mm_min > cap) {
        mm_min = cap;
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

/* Один тик РГИ → mpg_cmd; lim_bypass=1 — без clamp лимитов (Rapid LIVE) */
static void mpg_apply_tick(uint8_t axis, int32_t steps, uint8_t lim_bypass, uint8_t *lim_hit) {
    int32_t prev;
    int32_t raw;

    if (steps == 0) return;

    prev = mpg_cmd[axis];
    raw = mpg_cmd[axis] + steps;
    mpg_cmd[axis] = jog_clamp_target(axis, raw, lim_bypass);
    if (lim_hit != NULL && !lim_bypass && mpg_cmd[axis] != raw) {
        *lim_hit = 1U;
    }
    hand_pos[axis] += mpg_cmd[axis] - prev;
}

static void mpg_sync_cmd(void) {  /* mpg_cmd = текущая позиция DDS */
    mpg_cmd[AXIS_X] = dds_get_position(AXIS_X);
    mpg_cmd[AXIS_Z] = dds_get_position(AXIS_Z);
}

static int32_t mpg_clamp_cmd_ahead(int32_t pos, int32_t cmd_tgt) {
    int32_t d = cmd_tgt - pos;

    if (d > MPG_MAX_CMD_AHEAD) return pos + MPG_MAX_CMD_AHEAD;
    if (d < -MPG_MAX_CMD_AHEAD) return pos - MPG_MAX_CMD_AHEAD;
    return cmd_tgt;
}

static void mpg_session_halt(void) {  /* полный стоп MPG/jog + sync cmd */
    planner_jog_halt();
    mpg_active = 0;
    mpg_dir_lock = 0;
    mpg_rev_cnt = 0;
    mpg_batch_ms = 0;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
    mpg_rapid_prev = 0;
    mpg_approach_arm = 0;
    mpg_approach_ticks = 0;
    mpg_approach_go = 0;
#endif
    mpg_sync_cmd();
}

#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
/* Отпускание Rapid → подвод к mpg_cmd (cruise: без хвоста торможения) */
static void mpg_approach_release(uint8_t axis, const SwitchState_t *sw, const ButtonState_t *btn) {
    int32_t pos;
    int32_t tx;
    int32_t tz;
    int32_t cmd;
    uint8_t lim_hit = 0U;

    (void)sw;
    (void)btn;
    if (axis > AXIS_Z) return;

    pos = dds_get_position(axis);
    cmd = jog_clamp_target(axis, mpg_cmd[axis], 0);
    if (cmd != mpg_cmd[axis]) {
        lim_hit = 1U;
        mpg_cmd[axis] = cmd;
    }
    if (pos == cmd) {
        mpg_dir_lock = 0;
        mpg_rev_cnt = 0;
        mpg_approach_go = 0;
        return;
    }

    tx = dds_get_position(AXIS_X);
    tz = dds_get_position(AXIS_Z);
    if (axis == AXIS_X) {
        tx = cmd;
    } else {
        tz = cmd;
    }
    /* cruise=1: равномерно до цели, без медленного хвоста decel профиля */
    if (planner_exec_jog(tx, tz, mpg_speed_mm_min(axis, 0, 0, 1), "MPG", 1, lim_hit, 0U, 0L)) {
        mpg_active = 1;
        mpg_approach_go = 1;
        mpg_dir_lock = 0;
        mpg_rev_cnt = 0;
        mpg_batch_ms = 0;
        mpg_last_ms = millis();
        ui_encoder_discard_mpg_delta();
        DBG_MPG_PULSE(axis, mpg_cmd[axis]);
        DBG_INFO("JOG", "MPG", "approach go");
    }
}

/* Подвод активен: ждать pos==cmd, стоп без coast/lead/bl_drain */
static uint8_t mpg_approach_go_poll(void) {
    uint8_t axis = mpg_axis_last;
    int32_t pos;
    int32_t cmd;

    if (!mpg_approach_go) return 0U;

    ui_encoder_discard_mpg_delta();
    pos = dds_get_position(axis);
    cmd = mpg_cmd[axis];
    if (pos != cmd) return 1U;  /* ещё едем */

    /* На цели — halt (abort rem), без planner_jog_stop → bl_drain */
    planner_jog_halt();
    mpg_cmd[axis] = dds_get_position(axis);
    mpg_approach_go = 0;
    mpg_active = 0;
    mpg_dir_lock = 0;
    mpg_rev_cnt = 0;
    mpg_batch_ms = 0;
    DBG_INFO("JOG", "MPG", "approach done");
    return 1U;
}

/* Фронты Rapid для РГИ APPROACH. Джойстик Rapid — отдельно, сюда не входит. */
static void mpg_approach_edges(const SwitchState_t *sw, const ButtonState_t *btn) {
    uint8_t axis = sw->mpg_axis;
    uint8_t joy_any = (uint8_t)(joy_z_on | joy_x_on);

    /* Joy+Rapid: снять arm без тиков — APPROACH только для РГИ */
    if (btn->joy_rapid && mpg_approach_arm && joy_any && !mpg_approach_ticks) {
        mpg_approach_arm = 0;
        mpg_approach_go = 0;
        mpg_active = 0;
        mpg_dir_lock = 0;
        mpg_rev_cnt = 0;
        mpg_batch_ms = 0;
        mpg_cmd[mpg_axis_last] = dds_get_position(mpg_axis_last);
        DBG_INFO("JOG", "MPG", "arm joy");
    }

    if (btn->joy_rapid && !mpg_rapid_prev) {
        if (joy_any) {
            /* Rapid для джойстика — не армим APPROACH */
            goto mpg_approach_edges_done;
        }
        if (mpg_approach_go || (mpg_active && dds_motion_busy())) {
            planner_jog_halt();
            mpg_approach_go = 0;
        }
        hand_pos[axis] = 0;
        mpg_cmd[axis] = dds_get_position(axis);
        mpg_dir_lock = 0;
        mpg_rev_cnt = 0;
        mpg_batch_ms = 0;
        mpg_approach_arm = 1;
        mpg_approach_ticks = 0;
        mpg_active = 1;
        DBG_INFO("JOG", "MPG", "arm");
    } else if (!btn->joy_rapid && mpg_rapid_prev && mpg_approach_arm) {
        uint8_t had_ticks = mpg_approach_ticks;
        uint8_t axis_r = mpg_axis_last;
        int32_t pos = dds_get_position(axis_r);

        mpg_approach_arm = 0;
        mpg_approach_ticks = 0;
        mpg_batch_ms = 0;

        /* Без тиков РГИ — только sync, без хода */
        if (!had_ticks) {
            mpg_cmd[axis_r] = pos;
            mpg_active = 0;
            mpg_dir_lock = 0;
            mpg_rev_cnt = 0;
            mpg_approach_go = 0;
            goto mpg_approach_edges_done;
        }

        mpg_approach_release(axis_r, sw, btn);
    }
mpg_approach_edges_done:
    mpg_rapid_prev = btn->joy_rapid;
}
#endif

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

/* Lead в шагах: ~2·JOY_STEP_MS хода на скорости РГИ */
static int32_t mpg_lead_steps(uint8_t axis, uint8_t mpg_scale, uint8_t rapid) {
    float mm_min = mpg_speed_mm_min(axis, mpg_scale, rapid, 0);
    float spm = (axis == AXIS_X) ? STEPS_PER_MM_X : STEPS_PER_MM_Z;
    int32_t lead = (int32_t)((mm_min * spm * (float)(JOY_STEP_MS * 2U)) / 60000.0f);

    if (lead < 8) lead = 8;
    if (lead > MPG_MAX_LEAD) lead = MPG_MAX_LEAD;
    return lead;
}

/*
 * Цель planner: mpg_cmd, опционально + lead ЗА cmd (не от pos!).
 * pad = pos+lead каждый poll давал runaway в одну сторону.
 */
static void mpg_planner_commit(uint8_t axis, const SwitchState_t *sw, const ButtonState_t *btn,
                               uint8_t lim_hit, uint8_t use_lead) {
    TRACE_ENTER(TR_MPG_COMMIT);
    int32_t tx;
    int32_t tz;
    int32_t pos;
    int32_t cmd_tgt;
    uint8_t was_cruise;

    tx = dds_get_position(AXIS_X);
    tz = dds_get_position(AXIS_Z);
    pos = (axis == AXIS_X) ? tx : tz;
    cmd_tgt = mpg_cmd[axis];

    if (use_lead && mpg_dir_lock != 0) {
        cmd_tgt = mpg_cmd[axis] + (int32_t)mpg_dir_lock *
                  mpg_lead_steps(axis, sw->mpg_scale, btn->joy_rapid);
    }

    /* не командовать назад относительно pos (без записи в mpg_cmd) */
    if (mpg_dir_lock > 0 && cmd_tgt < pos) {
        cmd_tgt = pos;
    } else if (mpg_dir_lock < 0 && cmd_tgt > pos) {
        cmd_tgt = pos;
    }
    /* обгон cmd из‑за lead: не тянуть назад к cmd */
    if (!use_lead) {
        if (mpg_dir_lock > 0 && pos > mpg_cmd[axis]) {
            cmd_tgt = pos;
        } else if (mpg_dir_lock < 0 && pos < mpg_cmd[axis]) {
            cmd_tgt = pos;
        }
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
        mpg_active = 1;
        return;
    }

    was_cruise = dds_motion_jog_cruise_active();
    if (planner_exec_jog(tx, tz, mpg_speed_mm_min(axis, sw->mpg_scale, btn->joy_rapid, 0),
                         "MPG", 1, lim_hit, 0U, 0L)) {
        mpg_active = 1;
        if (!was_cruise) {
            DBG_MPG_PULSE(axis, mpg_cmd[axis]);
        }
    } else if (was_cruise) {
        mpg_active = 1;
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
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
    mpg_rapid_prev = 0;
    mpg_approach_arm = 0;
    mpg_approach_ticks = 0;
    mpg_approach_go = 0;
#endif
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
    /* люфт не трогаем: обнуление — только координата */
}

void motion_jog_go_limit(uint8_t idx) {
    uint8_t axis;
    int32_t target;

    if (estop_is_triggered()) return;
    if (ui_switches_mode_off()) return;
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
    if (ui_switches_mode_off()) return;
    if (!limits_ui_go_target(idx, &axis, &target)) return;
    if (dds_get_position(axis) == target) return;

    go_lim_axis = axis;
    go_lim_target = target;
    go_lim_active = 1;
    go_lim_latch = 1;
    go_lim_joy_arm = 0;

    hal_buzzer_beep_ms(40);  /* подтверждение входа в защёлку */
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
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
    mpg_rapid_prev = 0;
    mpg_approach_arm = 0;
    mpg_approach_ticks = 0;
    mpg_approach_go = 0;
#endif
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
    if (ui_switches_mode_off()) {
        if (joy_run) {
            DBG_INFO("JOG", "JOY", "blk MODE_OFF");
            joy_run = 0;
        }
        joy_z_on = 0;
        joy_x_on = 0;
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
    /* Стоп go_lim джойстиком: только стоп; jog — после отпускания и нового включения */
    if (go_lim_stop_joy) {
        if (!ui_buttons_feed_joy_on()) {
            go_lim_stop_joy = 0;
            joy_z_on = 0;
            joy_x_on = 0;
        }
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
        if ((was_z_on || was_x_on)) {
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
    uint8_t need_exec = 0U;

    if (z_on) {
        int32_t tgt_z = dds_get_target(AXIS_Z);
        int32_t rem_z = (z_sign > 0) ? (tgt_z - tz) : ((z_sign < 0) ? (tz - tgt_z) : 0);
        if (rem_z < JOY_RUNWAY_REFRESH) {
            int32_t base_z = (rem_z > 0) ? tgt_z : tz;
            tz = jog_clamp_target(AXIS_Z, base_z + z_sign * JOY_RUNWAY_FILL, btn.joy_rapid);
            need_exec = 1U;
        } else {
            tz = tgt_z;
        }
    }
    if (x_on) {
        int32_t tgt_x = dds_get_target(AXIS_X);
        int32_t rem_x = (x_sign > 0) ? (tgt_x - tx) : ((x_sign < 0) ? (tx - tgt_x) : 0);
        if (rem_x < JOY_RUNWAY_REFRESH) {
            int32_t base_x = (rem_x > 0) ? tgt_x : tx;
            tx = jog_clamp_target(AXIS_X, base_x + x_sign * JOY_RUNWAY_FILL, btn.joy_rapid);
            need_exec = 1U;
        } else {
            tx = tgt_x;
        }
    }

    /* Лимит/цель=позиция: cruise не гасим — только обновить скорость с пота (7e2 aFeed) */
    if (tx == ox && tz == oz) {
        if (dds_motion_jog_cruise_active() && !mpg_active) {
            planner_exec_jog(tx, tz, jog_speed_dual_mm_min(btn.joy_rapid), "JOY", 1, 0U, 0U, 0L);
        }
        return;
    }

    if (!need_exec) return;

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
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
        mpg_rapid_prev = 0;
        mpg_approach_arm = 0;
        mpg_approach_ticks = 0;
        mpg_approach_go = 0;
#endif
        return;
    }
    if (ui_switches_mode_off()) {
        mpg_active = 0;
        mpg_rev_cnt = 0;
        mpg_dir_lock = 0;
        mpg_batch_ms = 0;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
        mpg_rapid_prev = 0;
        mpg_approach_arm = 0;
        mpg_approach_ticks = 0;
        mpg_approach_go = 0;
#endif
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

    *sw = ui_switches_get_state();
    *btn = ui_buttons_get_state();
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
    mpg_approach_edges(sw, btn);
    if (mpg_approach_go_poll()) return;
#endif

    /* JOY на оси РГИ — не commit/coast к mpg_cmd (иначе реверс ±4096 в логе) */
    if ((sw->mpg_axis == AXIS_Z && joy_z_on) || (sw->mpg_axis == AXIS_X && joy_x_on)) {
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
        if (mpg_approach_go) {
            planner_jog_halt();
            mpg_approach_go = 0;
        }
#endif
        if (mpg_active) {
            mpg_active = 0;
            mpg_batch_ms = 0;
            mpg_dir_lock = 0;
            mpg_rev_cnt = 0;
        }
        ui_encoder_discard_mpg_delta();
        return;
    }

    if (ui_encoder_peek_mpg_delta() == 0) {
        if (mpg_rapid_deferred(btn)) {
            return;
        }
        if (mpg_batch_ms != 0UL && (millis() - mpg_batch_ms) >= MPG_BATCH_MS) {
            mpg_batch_ms = 0UL;
            mpg_planner_commit(mpg_axis_last, sw, btn, 0U, 1U);
            mpg_last_ms = millis();
            return;
        }
        /*
         * Lead только = mpg_cmd±pad (фиксирован к cmd). Не refresh от pos.
         * После COAST — цель на cmd/pos без отката; idle → stop.
         */
        if (mpg_active) {
            int32_t pos = dds_get_position(mpg_axis_last);
            int32_t cmd = mpg_cmd[mpg_axis_last];
            unsigned long age = millis() - mpg_last_ms;

            if (age < MPG_IDLE_MS) {
                if (age >= MPG_COAST_MS && pos != cmd &&
                    dds_get_target(mpg_axis_last) != cmd) {
                    mpg_planner_commit(mpg_axis_last, sw, btn, 0U, 0U);
                }
                return;
            }
            if (pos != cmd) {
                mpg_planner_commit(mpg_axis_last, sw, btn, 0U, 0U);
                return;
            }
            if (dds_motion_jog_cruise_active() || dds_motion_busy()) {
                planner_jog_stop();
            }
            mpg_cmd[mpg_axis_last] = dds_get_position(mpg_axis_last);
            mpg_active = 0;
            mpg_rev_cnt = 0;
            mpg_batch_ms = 0;
            ui_encoder_discard_mpg_delta();
        }
        return;
    }

    axis = sw->mpg_axis;

    if ((axis == AXIS_Z && joy_z_on) || (axis == AXIS_X && joy_x_on)) {
        DBG_VERBOSE("JOG", "MPG", "blk joy");
        return;
    }

    /* любая активность энкодера продлевает сессию (в т.ч. игнор реверса) */
    mpg_last_ms = millis();
    if (dds_motion_jog_cruise_active()) {
        mpg_active = 1;
    }

    {
        int8_t lock_sign = mpg_dir_lock;
        uint8_t preview = mpg_rapid_deferred(btn);

        for (uint8_t n = 0; n < MPG_MAX_TICKS; n++) {
            int8_t tick_sign;
            int32_t peek_d;
            uint8_t lim_bypass;

            peek_d = ui_encoder_peek_mpg_delta();
            if (peek_d == 0) break;
            tick_sign = (peek_d > 0) ? 1 : -1;
            tick_sign = mpg_adjust_tick_sign(axis, tick_sign);
            lim_bypass = mpg_lim_bypass(btn);

            if (preview) {
                steps = jog_steps_from_delta(axis, tick_sign, sw->mpg_scale,
                                             mpg_step_use_rapid(btn));
                (void)ui_encoder_consume_mpg_tick();
                mpg_apply_tick(axis, steps, lim_bypass, &lim_hit);
                if (steps != 0) {
                    any_tick = 1U;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
                    mpg_approach_ticks = 1U;
#endif
                }
                continue;
            }

            if (lock_sign == 0) {
                lock_sign = tick_sign;
                mpg_dir_lock = tick_sign;
                mpg_rev_cnt = 0;
            } else if (tick_sign != lock_sign) {
                (void)ui_encoder_consume_mpg_tick();
                /*
                 * ТЗ: ≤N игнор, >N полный стоп — иначе «всегда одна сторона».
                 */
                if (mpg_active || dds_motion_jog_cruise_active()) {
                    mpg_rev_cnt++;
                    if (mpg_rev_cnt <= MPG_REV_IGNORE_N) {
                        continue;
                    }
                    if (dds_motion_jog_cruise_active() || dds_motion_busy()) {
                        planner_jog_stop();
                    }
                    mpg_cmd[axis] = dds_get_position(axis);
                    mpg_active = 0;
                    mpg_dir_lock = 0;
                    mpg_rev_cnt = 0;
                    mpg_batch_ms = 0;
                    ui_encoder_discard_mpg_delta();
                    return;
                }
                mpg_rev_cnt++;
                if (mpg_rev_cnt <= MPG_REV_IGNORE_N) {
                    continue;
                }
                lock_sign = tick_sign;
                mpg_dir_lock = tick_sign;
                mpg_rev_cnt = 0;
                steps = jog_steps_from_delta(axis, tick_sign, sw->mpg_scale,
                                             mpg_step_use_rapid(btn));
                mpg_apply_tick(axis, steps, lim_bypass, &lim_hit);
                if (steps != 0) any_tick = 1U;
                continue;
            } else {
                mpg_rev_cnt = 0;
            }

            steps = jog_steps_from_delta(axis, tick_sign, sw->mpg_scale,
                                         mpg_step_use_rapid(btn));
            (void)ui_encoder_consume_mpg_tick();
            mpg_apply_tick(axis, steps, lim_bypass, &lim_hit);
            if (steps != 0) any_tick = 1U;
        }
    }

    if (!any_tick) return;

    mpg_active = 1;
    mpg_last_ms = millis();
    if (mpg_batch_ms == 0UL) {
        mpg_batch_ms = mpg_last_ms;
    }

    if (mpg_rapid_deferred(btn)) {
        return;
    }

    if (ui_encoder_peek_mpg_delta() != 0 &&
        (millis() - mpg_batch_ms) < MPG_BATCH_MS) {
        return;
    }
    mpg_batch_ms = 0UL;

    mpg_planner_commit(axis, sw, btn, lim_hit, 1U);
}
