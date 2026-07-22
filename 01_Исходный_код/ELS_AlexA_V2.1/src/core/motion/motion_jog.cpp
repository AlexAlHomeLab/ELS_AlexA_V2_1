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
#include "../ui/ui_menu.h"
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
#define MPG_REV_IGNORE_N 10        /* ТЗ: реверс ≤N игнор, >N — halt */
#define MPG_MAX_CMD_AHEAD 4096    /* макс. опережение цели над position, шаги */
#define MPG_MAX_LEAD 128          /* pad за mpg_cmd (не от pos — иначе runaway) */
#define MPG_IDLE_MS 2500UL        /* конец сессии: нет тиков ≥ IDLE */
#define MPG_COAST_MS 300UL        /* после тика: держим pad за cmd, без refresh от pos */
#define MPG_BATCH_MS 8UL          /* пакет детентов перед первым commit */
#define MPG_AXIS_ARM_LOOPS 2U     /* пропуск poll после смены оси РГИ */
#define SC_COOLDOWN_MS 300UL      /* тишина энкодера после release/zero перед MPG */

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
static uint8_t mpg_coast_logged;  /* 1 — VERBOSE coast уже был в этой сессии */
static uint8_t mpg_idle_catch_logged; /* 1 — VERBOSE idle catch уже был */
static uint8_t mpg_hold_tgt_logged;   /* 1 — VERBOSE hold tgt уже был */
static uint8_t mpg_cruise_keep_logged; /* 1 — VERBOSE cruise keep уже был */
static int32_t mpg_lead_pos0;     /* pos на холодном старте — lead только после сдвига */
static uint8_t mpg_lead_ok;       /* 1 — реальный ход начался (BL закончен / не нужен) */
static SwitchState_t mpg_sw;
static ButtonState_t mpg_btn;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
static uint8_t mpg_rapid_prev;    /* предыдущее состояние Rapid для фронтов */
static uint8_t mpg_approach_arm;  /* 1 — Rapid удерживается, движение отложено */
static uint8_t mpg_approach_ticks; /* 1 — за удержание Rapid были тики РГИ */
static uint8_t mpg_approach_go;   /* 1 — идёт подвод к mpg_cmd после отпускания Rapid */
#endif
/* Установка координаты: hold L/R/U/D + РГИ без хода (els-display-menu / els-rgi-mpg) */
static uint8_t sc_btn;            /* 0=нет, 1=Left, 2=Right, 3=Up, 4=Down */
static uint8_t sc_axis;           /* AXIS_X / AXIS_Z */
static uint8_t sc_frac;           /* 0 — целая часть LCD, 1 — дробная (0.001) */
static int32_t sc_pos;            /* превью позиции, шаги */
static uint8_t sc_dirty;          /* 1 — были тики РГИ за удержание */
static uint8_t sc_cool_on;        /* 1 — антидребезг после release/zero */
static unsigned long sc_cool_ms;  /* millis() старта cooldown */

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
    DBG_INFO_VAL("JOG", "MPG", "hand rst", (uint32_t)axis);
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

/* Set-coord Xdia=D: знак тика РГИ (крутим + → D на LCD растёт). */
static int8_t sc_diameter_tick_sign(uint8_t axis, int8_t tick_sign) {
    tick_sign = mpg_adjust_tick_sign(axis, tick_sign);
    if (axis == AXIS_X && config_get_x_coord_mode() == X_COORD_MODE_DIAMETER) {
        tick_sign = (int8_t)(-tick_sign);
    }
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
        /* один раз на «упор» (повторные тики в ту же cmd не спамят) */
        {
            static int32_t lim_cmd_log[2] = { 0x7FFFFFFF, 0x7FFFFFFF };
            if (mpg_cmd[axis] != lim_cmd_log[axis]) {
                lim_cmd_log[axis] = mpg_cmd[axis];
                DBG_INFO_VAL_I32("JOG", "MPG", "lim", mpg_cmd[axis]);
            }
        }
    }
    hand_pos[axis] += mpg_cmd[axis] - prev;
}

static void mpg_sync_cmd(void) {  /* mpg_cmd = текущая позиция DDS */
    mpg_cmd[AXIS_X] = dds_get_position(AXIS_X);
    mpg_cmd[AXIS_Z] = dds_get_position(AXIS_Z);
}

/* Сброс хвоста РГИ по оси (peer при старте joy на другой оси) */
static void mpg_detach_axis(uint8_t axis) {
    int32_t p;

    if (axis > AXIS_Z) return;
    p = dds_get_position(axis);
    mpg_cmd[axis] = p;
    dds_set_target(axis, p);
    if (mpg_axis_last != axis) return;
    mpg_active = 0;
    mpg_rev_cnt = 0;
    mpg_batch_ms = 0;
    mpg_coast_logged = 0;
    mpg_idle_catch_logged = 0;
    mpg_hold_tgt_logged = 0;
    mpg_cruise_keep_logged = 0;
    mpg_lead_ok = 0;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
    mpg_approach_arm = 0;
    mpg_approach_ticks = 0;
    mpg_approach_go = 0;
#endif
}

static void joy_mpg_isolate_peer(uint8_t started_axis) {
    uint8_t peer = (started_axis == AXIS_X) ? AXIS_Z : AXIS_X;

    mpg_detach_axis(peer);
}

/* JOY: peer-оси без джойстика — цель = позиция (одноосный ход в planner) */
static void planner_exec_joy(int32_t tx, int32_t tz, float speed_mm_min) {
    if (!joy_z_on) {
        tz = dds_get_position(AXIS_Z);
    }
    if (!joy_x_on) {
        tx = dds_get_position(AXIS_X);
    }
    planner_exec_jog(tx, tz, speed_mm_min, "JOY", 1, 0U, 0U, 0L);
}

/* Шаг правки координаты на 1 тик РГИ: целая = 1.000 ед. LCD, дробная = 0.001.
 * Xdia=диаметр: половина машинных шагов, чтобы на LCD шаг совпал с селектором. */
static int32_t set_coord_tick_steps(uint8_t axis, uint8_t frac) {
    uint8_t units = config_get_coord_units();
    uint8_t diam = (axis == AXIS_X &&
                    config_get_x_coord_mode() == X_COORD_MODE_DIAMETER) ? 1U : 0U;
    int32_t spm = (int32_t)((axis == AXIS_X) ? STEPS_PER_MM_X : STEPS_PER_MM_Z);
    int32_t step;

    if (units == COORD_UNIT_STEPS) {
        step = frac ? 1L : 1000L;
    } else if (units == COORD_UNIT_INCH) {
        /* 1″ = 25.4 мм; дробная — 0.001″ */
        if (frac) {
            step = (int32_t)((spm * 254L + 5000L) / 10000L);
        } else {
            step = (int32_t)((spm * 254L + 5L) / 10L);
        }
    } else {
        /* мм: 1.000 / 0.001 */
        step = frac ? (spm / 1000L) : spm;
    }
    if (diam && units != COORD_UNIT_STEPS) {
        step /= 2L;
    }
    if (step < 1L) step = 1L;
    return step;
}

/* Шаги ↔ тысячные доли отображения (как CrdU/Xdia на LCD). */
static int32_t sc_steps_to_val1000(uint8_t axis, int32_t steps) {
    uint8_t units = config_get_coord_units();
    uint8_t diam = (axis == AXIS_X &&
                    config_get_x_coord_mode() == X_COORD_MODE_DIAMETER) ? 1U : 0U;
    int32_t spm = (int32_t)((axis == AXIS_X) ? STEPS_PER_MM_X : STEPS_PER_MM_Z);
    int64_t num;
    int64_t den;

    if (spm < 1) spm = 1;
    if (units == COORD_UNIT_INCH) {
        /* steps → 0.001″: / (spm*25.4) * 1000 = *10000/(spm*254) */
        num = (int64_t)steps * 10000LL;
        den = (int64_t)spm * 254LL;
        if (diam) num *= 2LL;
        if (num >= 0) return (int32_t)((num + den / 2) / den);
        return (int32_t)((num - den / 2) / den);
    }
    /* мм: steps → 0.001 мм */
    num = (int64_t)steps * 1000LL;
    if (diam) num *= 2LL;
    den = (int64_t)spm;
    if (num >= 0) return (int32_t)((num + den / 2) / den);
    return (int32_t)((num - den / 2) / den);
}

static int32_t sc_val1000_to_steps(uint8_t axis, int32_t val1000) {
    uint8_t units = config_get_coord_units();
    uint8_t diam = (axis == AXIS_X &&
                    config_get_x_coord_mode() == X_COORD_MODE_DIAMETER) ? 1U : 0U;
    int32_t spm = (int32_t)((axis == AXIS_X) ? STEPS_PER_MM_X : STEPS_PER_MM_Z);
    int64_t num;
    int64_t den;

    if (spm < 1) spm = 1;
    if (units == COORD_UNIT_INCH) {
        /* 0.001″ → steps: *spm*25.4/1000 = *spm*254/10000 */
        num = (int64_t)val1000 * (int64_t)spm * 254LL;
        den = 10000LL;
        if (diam) den *= 2LL;
        if (num >= 0) return (int32_t)((num + den / 2) / den);
        return (int32_t)((num - den / 2) / den);
    }
    num = (int64_t)val1000 * (int64_t)spm;
    den = 1000LL;
    if (diam) den *= 2LL;
    if (num >= 0) return (int32_t)((num + den / 2) / den);
    return (int32_t)((num - den / 2) / den);
}

/* Антидребезг после set-coord release / zero оси: discard тиков, блок MPG.
 * Таймер продлевается, пока с энкодера ещё приходят тики (нет «тихого» окна). */
static void sc_cooldown_arm(void) {
    sc_cool_on = 1U;
    sc_cool_ms = millis();
    ui_encoder_discard_mpg_delta();
    mpg_active = 0;
    /* dir_lock не трогаем — иначе после zero/release нет rev ign N перед BL */
    mpg_rev_cnt = 0;
    mpg_batch_ms = 0;
    mpg_coast_logged = 0;
    mpg_idle_catch_logged = 0;
    if (dds_motion_busy() || dds_motion_jog_cruise_active()) {
        planner_jog_halt();
    }
    mpg_sync_cmd();
}

static uint8_t sc_cooldown_active(void) {
    if (!sc_cool_on) return 0U;
    /* Пока есть тики — съедать и ждать полную тишину SC_COOLDOWN_MS */
    if (ui_encoder_peek_mpg_delta() != 0) {
        ui_encoder_discard_mpg_delta();
        sc_cool_ms = millis();
        return 1U;
    }
    if ((millis() - sc_cool_ms) < SC_COOLDOWN_MS) {
        return 1U;
    }
    sc_cool_on = 0U;
    ui_encoder_discard_mpg_delta();
    mpg_sync_cmd();
    DBG_INFO("JOG", "SC", "cool end");
    return 0U;
}

/* Блок MPG при дребезге/двух кнопках (ещё не id, но линия уже low) */
static void sc_mpg_gate(void) {
    ui_encoder_discard_mpg_delta();
    mpg_active = 0;
    mpg_dir_lock = 0;
    mpg_rev_cnt = 0;
    mpg_batch_ms = 0;
    if (dds_motion_busy() || dds_motion_jog_cruise_active()) {
        planner_jog_halt();
    }
    mpg_sync_cmd();
}

/* Один тик set-coord: X — инверсия знака; мм/″ — шаг set_coord_tick_steps (не val1000±1). */
static void set_coord_apply_tick(int8_t tick_sign) {
    int32_t step;

    if (sc_axis == AXIS_X && config_get_x_coord_mode() == X_COORD_MODE_DIAMETER) {
        tick_sign = sc_diameter_tick_sign(sc_axis, tick_sign);
        step = set_coord_tick_steps(sc_axis, sc_frac);
        sc_pos -= (int32_t)tick_sign * step;
        return;
    }

    tick_sign = mpg_adjust_tick_sign(sc_axis, tick_sign);
    if (sc_axis == AXIS_X) {
        tick_sign = (int8_t)(-tick_sign);
    }

    step = set_coord_tick_steps(sc_axis, sc_frac);
    sc_pos += (int32_t)tick_sign * step;
}

/* Commit превью: смена отображаемой координаты без хода + rebase лимитов */
static void set_coord_commit(void) {
    int32_t old;
    uint8_t axis = sc_axis;

    if (axis > AXIS_Z) return;
    old = dds_get_position(axis);
    if (sc_pos == old) {
        mpg_cmd[axis] = old;
        DBG_INFO("JOG", "SC", "commit same");
        return;
    }
    /* limit_pos += (new - old) ≡ rebase(old - new) */
    limits_rebase_axis(axis, old - sc_pos);
    motion_set_pos_steps(axis, sc_pos);
    mpg_cmd[axis] = sc_pos;
    DBG_INFO_VAL("JOG", "SC", "commit", (uint32_t)axis);
    DBG_INFO_VAL_I32("JOG", "SC", "pos", sc_pos);
}

/* Сброс сессии set-coord; discard_ticks=1 — съесть хвост РГИ (изоляция) */
static void set_coord_clear(uint8_t discard_ticks) {
    sc_btn = 0;
    sc_dirty = 0;
    if (discard_ticks) {
        ui_encoder_discard_mpg_delta();
    }
}

/*
 * Hold Left/Right (X) или Up/Down (Z) + РГИ: правка превью без хода.
 * btn_id: 1=L 2=R 3=U 4=D; Z: U=дробная, D=целая (X: L/R).
 * Возвращает 1 — тики РГИ заняты set-coord (jog не трогать).
 */
static uint8_t set_coord_poll(uint8_t btn_id) {
    uint8_t axis;
    uint8_t frac;
    uint8_t n;

    if (ui_menu_is_active()) {
        if (sc_btn) {
            set_coord_clear(1U);
            DBG_INFO("JOG", "SC", "abort menu");
        }
        return 0U;
    }

    if (btn_id == 0U) {
        if (sc_btn) {
            if (sc_dirty) {
                set_coord_commit();
            } else {
                DBG_INFO("JOG", "SC", "release idle");
            }
            set_coord_clear(1U);
            sc_cooldown_arm();  /* хвост тиков РГИ не должен стартовать MPG */
            DBG_INFO("JOG", "SC", "cool");
        }
        return 0U;
    }

    axis = (btn_id <= 2U) ? AXIS_X : AXIS_Z;
    /* X: L целая, R дробная; Z: Down целая, Up дробная */
    frac = (btn_id == 2U || btn_id == 3U) ? 1U : 0U;

    /* Joy/go_lim — нельзя править координату */
    if (go_lim_active || joy_z_on || joy_x_on) {
        if (sc_btn) {
            set_coord_clear(1U);
            DBG_INFO("JOG", "SC", "abort busy");
        }
        ui_encoder_discard_mpg_delta();
        return 1U;
    }

    if (sc_btn != btn_id) {
        if (sc_btn && sc_dirty) {
            DBG_INFO("JOG", "SC", "switch drop");
        }
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
        if (mpg_approach_arm || mpg_approach_go) {
            mpg_approach_arm = 0;
            mpg_approach_ticks = 0;
            mpg_approach_go = 0;
        }
#endif
        /* Стоп любого MPG/coast и sync cmd — иначе idle catch снова крутит моторы */
        if (mpg_active || dds_motion_jog_cruise_active() || dds_motion_busy()) {
            planner_jog_halt();
        }
        mpg_active = 0;
        mpg_dir_lock = 0;
        mpg_rev_cnt = 0;
        mpg_batch_ms = 0;
        mpg_coast_logged = 0;
        mpg_idle_catch_logged = 0;
        mpg_sync_cmd();
        sc_btn = btn_id;
        sc_axis = axis;
        sc_frac = frac;
        sc_pos = dds_get_position(axis);
        sc_dirty = 0;
        ui_encoder_discard_mpg_delta();
        DBG_INFO_VAL("JOG", "SC", "arm", (uint32_t)btn_id);
        DBG_INFO_VAL("JOG", "SC", axis == AXIS_X ? "X" : "Z", (uint32_t)frac);
        DBG_INFO_VAL_I32("JOG", "SC", "step", set_coord_tick_steps(axis, frac));
    }

    {
        uint8_t got = 0U;
        for (n = 0; n < MPG_MAX_TICKS; n++) {
            int8_t tick_sign;
            int32_t peek_d = ui_encoder_peek_mpg_delta();
            if (peek_d == 0) break;
            tick_sign = (peek_d > 0) ? 1 : -1;
            (void)ui_encoder_consume_mpg_tick();
            set_coord_apply_tick(tick_sign);
            got = 1U;
        }
        if (got) {
            if (!sc_dirty) {
                DBG_INFO_VAL_I32("JOG", "SC", "tick", sc_pos);
            }
            sc_dirty = 1U;
        }
    }
    return 1U;
}

static int32_t mpg_clamp_cmd_ahead(int32_t pos, int32_t cmd_tgt) {
    int32_t d = cmd_tgt - pos;

    if (d > MPG_MAX_CMD_AHEAD) return pos + MPG_MAX_CMD_AHEAD;
    if (d < -MPG_MAX_CMD_AHEAD) return pos - MPG_MAX_CMD_AHEAD;
    return cmd_tgt;
}

/* Ось «чужого» jog: не подставлять pos — иначе retarget затирает cruise другого источника */
static int32_t jog_peer_axis_hold(uint8_t axis) {
    /* Joy на одной оси: peer не тянет хвост РГИ/target другой оси */
    if (joy_x_on || joy_z_on) {
        if ((axis == AXIS_X && !joy_x_on) || (axis == AXIS_Z && !joy_z_on)) {
            return dds_get_position(axis);
        }
    }
    if (axis == AXIS_X) {
        if (joy_x_on) return dds_get_target(AXIS_X);
        if (mpg_active && mpg_axis_last == AXIS_X) return dds_get_target(AXIS_X);
    } else if (axis == AXIS_Z) {
        if (joy_z_on) return dds_get_target(AXIS_Z);
        if (mpg_active && mpg_axis_last == AXIS_Z) return dds_get_target(AXIS_Z);
    }
    /* orphan-ход после смены оси РГИ — не срывать target через pos */
    if (dds_get_target(axis) != dds_get_position(axis)) {
        return dds_get_target(axis);
    }
    return dds_get_position(axis);
}

/* Сброс сессии РГИ без planner_jog_halt (смена оси не двигает моторы) */
static void mpg_session_soft_reset(void) {
    mpg_active = 0;
    mpg_dir_lock = 0;
    mpg_rev_cnt = 0;
    mpg_batch_ms = 0;
    mpg_coast_logged = 0;
    mpg_idle_catch_logged = 0;
    mpg_hold_tgt_logged = 0;
    mpg_cruise_keep_logged = 0;
    mpg_lead_ok = 0;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
    mpg_rapid_prev = 0;
    mpg_approach_arm = 0;
    mpg_approach_ticks = 0;
    mpg_approach_go = 0;
#endif
}

/* Код режима скорости РГИ: 0=x1, 1=0.01, 2=LIVE 0.1, 3=APPROACH-подвод */
static uint8_t mpg_fmode_code(const SwitchState_t *sw, const ButtonState_t *btn,
                              uint8_t approach) {
    if (approach) return 3U;
    if (mpg_step_use_rapid(btn)) return 2U;
    if (sw->mpg_scale) return 1U;
    return 0U;
}

static void mpg_log_fmode(const SwitchState_t *sw, const ButtonState_t *btn,
                          uint8_t approach) {
    static uint8_t prev = 0xFF;
    uint8_t code = mpg_fmode_code(sw, btn, approach);
    uint8_t axis = sw->mpg_axis;
    float spd;

    if (code == prev) return;
    prev = code;
    spd = mpg_speed_mm_min(axis, sw->mpg_scale, mpg_step_use_rapid(btn), approach);
    if (code == 0U) {
        DBG_INFO_VAL("JOG", "MPG", "F x1", (uint32_t)spd);
    } else if (code == 1U) {
        DBG_INFO_VAL("JOG", "MPG", "F 001", (uint32_t)spd);
    } else if (code == 2U) {
        DBG_INFO_VAL("JOG", "MPG", "F live", (uint32_t)spd);
    } else {
        DBG_INFO_VAL("JOG", "MPG", "F appr", (uint32_t)spd);
    }
}

static void mpg_log_cmd_hand(uint8_t axis, uint8_t ticks) {
    if (ticks > 0U) {
        DBG_VERBOSE_VAL("JOG", "MPG", "ticks", (uint32_t)ticks);
    }
    DBG_VERBOSE_VAL_I32("JOG", "MPG", "cmd", mpg_cmd[axis]);
    DBG_VERBOSE_VAL_I32("JOG", "MPG", "hand", hand_pos[axis]);
}

#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
/* Отпускание Rapid → подвод к mpg_cmd (cruise: без хвоста торможения) */
static void mpg_approach_release(uint8_t axis, const SwitchState_t *sw, const ButtonState_t *btn) {
    int32_t pos;
    int32_t tx;
    int32_t tz;
    int32_t cmd;
    uint8_t lim_hit = 0U;

    if (axis > AXIS_Z) return;

    pos = dds_get_position(axis);
    cmd = jog_clamp_target(axis, mpg_cmd[axis], 0);
    if (cmd != mpg_cmd[axis]) {
        lim_hit = 1U;
        mpg_cmd[axis] = cmd;
        DBG_INFO_VAL_I32("JOG", "MPG", "lim", cmd);
    }
    if (pos == cmd) {
        mpg_dir_lock = 0;
        mpg_rev_cnt = 0;
        mpg_approach_go = 0;
        DBG_INFO("JOG", "MPG", "approach at tgt");
        return;
    }

    tx = jog_peer_axis_hold(AXIS_X);
    tz = jog_peer_axis_hold(AXIS_Z);
    if (axis == AXIS_X) {
        tx = cmd;
    } else {
        tz = cmd;
    }
    mpg_log_fmode(sw, btn, 1U);
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
    } else {
        DBG_INFO("JOG", "MPG", "approach fail");
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
        /* hand_pos не сбрасываем — только joy (ТЗ / STAGE_2_2e) */
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
            DBG_INFO("JOG", "MPG", "arm sync");  /* Rapid↓ без тиков */
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

    ui_encoder_discard_mpg_delta();
    mpg_session_soft_reset();

    /*
     * Пустой cruise на цели (hold tgt): снять профиль без хода.
     * Едущий ход не трогаем — доедет сам; peer_hold сохранит target.
     */
    if (!planner_startup_busy() && dds_motion_busy()) {
        int32_t px = dds_get_position(AXIS_X);
        int32_t pz = dds_get_position(AXIS_Z);
        if (dds_get_target(AXIS_X) == px && dds_get_target(AXIS_Z) == pz) {
            dds_motion_stop();
            backlash_abort_pending();
        }
    }

    mpg_cmd[new_axis] = dds_get_position(new_axis);
    mpg_axis_last = new_axis;
    mpg_axis_arm = MPG_AXIS_ARM_LOOPS;
    DBG_INFO_VAL("JOG", "MPG", "axis", (uint32_t)new_axis);
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
    uint8_t mpg_rapid;
    uint8_t lim_bypass;

    pos = (axis == AXIS_X) ? dds_get_position(AXIS_X) : dds_get_position(AXIS_Z);
    tx = jog_peer_axis_hold(AXIS_X);
    tz = jog_peer_axis_hold(AXIS_Z);
    was_cruise = dds_motion_jog_cruise_active();
    mpg_rapid = mpg_step_use_rapid(btn);
    lim_bypass = mpg_lim_bypass(btn);

    /*
     * Pos обогнал cmd (старый lead/инерция): sync cmd.
     * Иначе медленные тики → hold tgt без хода, пока cmd не догонит pos.
     */
    if (mpg_dir_lock > 0 && pos > mpg_cmd[axis]) {
        mpg_cmd[axis] = pos;
    } else if (mpg_dir_lock < 0 && pos < mpg_cmd[axis]) {
        mpg_cmd[axis] = pos;
    }
    cmd_tgt = mpg_cmd[axis];

    /*
     * Lead только после сдвига pos (BL закончен) и если РГИ уже обгоняет мотор.
     * Иначе после BL/короткого хода pad=MAX_LEAD → ext cmd±128 → рывок.
     */
    if (!was_cruise) {
        mpg_lead_pos0 = pos;
        mpg_lead_ok = 0;
    } else if (!mpg_lead_ok) {
        if (backlash_pending(axis) <= 0 && pos != mpg_lead_pos0) {
            mpg_lead_ok = 1U;
        }
    }

    if (use_lead && was_cruise && mpg_dir_lock != 0 && mpg_lead_ok &&
        backlash_pending(axis) <= 0) {
        int32_t lead = mpg_lead_steps(axis, sw->mpg_scale, mpg_rapid);
        int32_t gap = (mpg_dir_lock > 0) ? (mpg_cmd[axis] - pos)
                                         : (pos - mpg_cmd[axis]);
        if (gap > lead) {
            cmd_tgt = mpg_cmd[axis] + (int32_t)mpg_dir_lock * lead;
        }
    }

    /* не командовать назад относительно pos */
    if (mpg_dir_lock > 0 && cmd_tgt < pos) {
        cmd_tgt = pos;
    } else if (mpg_dir_lock < 0 && cmd_tgt > pos) {
        cmd_tgt = pos;
    }
    cmd_tgt = mpg_clamp_cmd_ahead(pos, cmd_tgt);

    if (axis == AXIS_X) {
        tx = jog_clamp_target(AXIS_X, cmd_tgt, lim_bypass);
        if (!lim_hit && !lim_bypass && tx != cmd_tgt) lim_hit = 1U;
    } else {
        tz = jog_clamp_target(AXIS_Z, cmd_tgt, lim_bypass);
        if (!lim_hit && !lim_bypass && tz != cmd_tgt) lim_hit = 1U;
    }

    if (lim_hit) {
        DBG_VERBOSE("JOG", "MPG", "lim commit");
    }

    if (tx == dds_get_target(AXIS_X) && tz == dds_get_target(AXIS_Z)) {
        mpg_active = 1;
        if (!mpg_hold_tgt_logged) {
            DBG_VERBOSE("JOG", "MPG", "hold tgt");
            mpg_hold_tgt_logged = 1U;
        }
        return;
    }

    mpg_log_fmode(sw, btn, 0U);
    if (planner_exec_jog(tx, tz, mpg_speed_mm_min(axis, sw->mpg_scale, mpg_rapid, 0),
                         "MPG", 1, lim_hit, 0U, 0L)) {
        mpg_active = 1;
        mpg_hold_tgt_logged = 0;
        mpg_cruise_keep_logged = 0;
        if (!was_cruise) {
            DBG_MPG_PULSE(axis, mpg_cmd[axis]);
            DBG_INFO_VAL("JOG", "MPG", "run", (uint32_t)axis);
        } else {
            DBG_VERBOSE_VAL_I32("JOG", "MPG", "ext", cmd_tgt);
        }
    } else if (was_cruise) {
        mpg_active = 1;
        if (!mpg_cruise_keep_logged) {
            DBG_VERBOSE("JOG", "MPG", "cruise keep");
            mpg_cruise_keep_logged = 1U;
        }
    } else {
        DBG_INFO("JOG", "MPG", "commit fail");
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
    mpg_coast_logged = 0;
    mpg_idle_catch_logged = 0;
    mpg_hold_tgt_logged = 0;
    mpg_cruise_keep_logged = 0;
    mpg_lead_ok = 0;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
    mpg_rapid_prev = 0;
    mpg_approach_arm = 0;
    mpg_approach_ticks = 0;
    mpg_approach_go = 0;
#endif
    sc_btn = 0;
    sc_dirty = 0;
    sc_cool_on = 0;
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

/* Превью установки координаты для LCD; 1 — активно (axis/pos заполнены) */
uint8_t motion_jog_set_coord_preview(uint8_t *axis, int32_t *pos) {
    if (!sc_btn) return 0U;
    if (axis) *axis = sc_axis;
    if (pos) *pos = sc_pos;
    return 1U;
}

void motion_jog_reset_pos(uint8_t axis) {
    if (axis > AXIS_Z) return;
    hand_pos[axis] = 0;
}

void motion_jog_zero_axis(uint8_t axis) {
    if (axis > AXIS_Z) return;
    int32_t cur = motion_get_pos_steps(axis);
    int8_t keep_dir = 0;

    if (go_lim_active && go_lim_axis == axis) {
        DBG_INFO_VAL("JOG", "GOLIM", "zero cancel", (uint32_t)axis);
        go_lim_active = 0;
        go_lim_joy_arm = 0;
        go_lim_latch = 0;
    }
    /* иначе MPG coast/jog тянет к старому mpg_cmd → ложная выборка люфта */
    planner_jog_halt();
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
    mpg_approach_arm = 0;
    mpg_approach_ticks = 0;
    mpg_approach_go = 0;
#endif
    /* dir_lock сохранить: иначе после zero 1-й тик = смена DIR/BL без rev ign N */
    if (axis == mpg_axis_last) {
        keep_dir = mpg_dir_lock;
    }
    limits_rebase_axis(axis, cur);
    motion_set_pos_steps(axis, 0);
    hand_reset_axis(axis);
    if (axis == mpg_axis_last) {
        mpg_dir_lock = keep_dir;
        mpg_rev_cnt = 0;
    }
    ui_encoder_reset_mpg();
    mpg_sync_cmd();
    sc_btn = 0;
    sc_dirty = 0;
    sc_cooldown_arm();  /* антидребезг: не стартовать MPG от хвоста тиков */
    /* Hold zero: только координата — не sync люфта (иначе last_dir=REF без выборки) */
    DBG_INFO_VAL("JOG", "JOY", "zero", (uint32_t)axis);
}

void motion_jog_go_limit(uint8_t idx) {
    uint8_t axis;
    int32_t target;

    if (estop_is_triggered()) {
        DBG_INFO("JOG", "GOLIM", "blk estop");
        return;
    }
    if (ui_switches_mode_off()) {
        DBG_INFO("JOG", "GOLIM", "blk MODE_OFF");
        return;
    }
    if (!limits_ui_go_target(idx, &axis, &target)) {
        DBG_INFO_VAL("JOG", "GOLIM", "miss", (uint32_t)idx);
        return;
    }
    if (dds_get_position(axis) == target) {
        DBG_INFO_VAL("JOG", "GOLIM", "at tgt", (uint32_t)axis);
        return;
    }

    go_lim_axis = axis;
    go_lim_target = target;
    go_lim_active = 1;
    go_lim_latch = 0;
    go_lim_joy_arm = 1;

    planner_exec_axis(axis, target, jog_speed_mm_min(axis, 1), 0);
    DBG_INFO_VAL("JOG", "GOLIM", "axis", (uint32_t)axis);
    DBG_INFO_VAL_I32("JOG", "GOLIM", "tgt", target);
}

void motion_jog_go_limit_latch(uint8_t idx) {
    uint8_t axis;
    int32_t target;

    if (estop_is_triggered()) {
        DBG_INFO("JOG", "LATCH", "blk estop");
        return;
    }
    if (ui_switches_mode_off()) {
        DBG_INFO("JOG", "LATCH", "blk MODE_OFF");
        return;
    }
    if (!limits_ui_go_target(idx, &axis, &target)) {
        DBG_INFO_VAL("JOG", "LATCH", "miss", (uint32_t)idx);
        return;
    }
    if (dds_get_position(axis) == target) {
        DBG_INFO_VAL("JOG", "LATCH", "at tgt", (uint32_t)axis);
        return;
    }

    go_lim_axis = axis;
    go_lim_target = target;
    go_lim_active = 1;
    go_lim_latch = 1;
    go_lim_joy_arm = 0;

    hal_buzzer_beep_ms(40);  /* подтверждение входа в защёлку */
    planner_exec_axis(axis, target, jog_speed_mm_min(axis, 0), 0);
    DBG_INFO_VAL("JOG", "LATCH", "axis", (uint32_t)axis);
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
    mpg_coast_logged = 0;
    mpg_idle_catch_logged = 0;
    mpg_hold_tgt_logged = 0;
    mpg_cruise_keep_logged = 0;
    mpg_lead_ok = 0;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
    mpg_rapid_prev = 0;
    mpg_approach_arm = 0;
    mpg_approach_ticks = 0;
    mpg_approach_go = 0;
#endif
    sc_btn = 0;
    sc_dirty = 0;
    sc_cool_on = 0;
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
            if (!ui_buttons_feed_joy_on()) {
                go_lim_joy_arm = 1;
                DBG_INFO("JOG", "LATCH", "arm");  /* joy отпущен — ход до лимита */
            }
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

void motion_jog_joy_poll(void) {  /* джойстик: runway cruise jog */
    int32_t tx;
    int32_t tz;
    static uint8_t joy_run = 0;
    static uint8_t joy_rapid_prev = 0;
    static uint8_t joy_blk_lim_log = 0;
    static uint8_t joy_blk_stop_log = 0;
    static uint16_t joy_spd_log = 0;

    if (estop_is_triggered()) {
        if (joy_run) {
            DBG_INFO("JOG", "JOY", "blk estop");
            joy_run = 0;
        }
        go_lim_active = 0;
        go_lim_latch = 0;
        go_lim_joy_arm = 0;
        joy_rapid_prev = 0;
        joy_blk_lim_log = 0;
        joy_spd_log = 0;
        return;
    }
    if (ui_switches_mode_off()) {
        if (joy_run) {
            DBG_INFO("JOG", "JOY", "blk MODE_OFF");
            joy_run = 0;
        }
        joy_z_on = 0;
        joy_x_on = 0;
        joy_rapid_prev = 0;
        joy_blk_lim_log = 0;
        joy_spd_log = 0;
        return;
    }

    motion_jog_limit_poll();
    if (go_lim_active) {
        if (joy_run) {
            DBG_INFO("JOG", "JOY", "blk golim");
            joy_run = 0;
        }
        joy_rapid_prev = 0;
        return;
    }
    /* Стоп go_lim джойстиком: только стоп; jog — после отпускания и нового включения */
    if (go_lim_stop_joy) {
        if (!ui_buttons_feed_joy_on()) {
            go_lim_stop_joy = 0;
            joy_z_on = 0;
            joy_x_on = 0;
            joy_blk_stop_log = 0;
            DBG_INFO("JOG", "JOY", "stop_joy clr");  /* H→I: можно новый жест */
        } else if (!joy_blk_stop_log) {
            DBG_INFO("JOG", "JOY", "blk stop_joy");  /* H: jog заблокирован */
            joy_blk_stop_log = 1;
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

    /* Rapid A↔B: фронты при удержании направления */
    if (joy_any) {
        if (btn.joy_rapid && !joy_rapid_prev) {
            DBG_INFO("JOG", "JOY", "rapid on");
        } else if (!btn.joy_rapid && joy_rapid_prev) {
            DBG_INFO("JOG", "JOY", "rapid off");
        }
    }
    joy_rapid_prev = btn.joy_rapid;

    if (joy_any && !joy_run) {
        DBG_INFO_VAL("UI", "JOY", "run",
                     (uint32_t)((z_sign + 2) | ((x_sign + 2) << 2) |
                                (btn.joy_rapid ? 16U : 0U)));
        DBG_INFO_VAL("JOG", "JOY", "mode", (uint32_t)fsm_manager_get_mode());
        if (z_on && x_on) {
            DBG_INFO("JOG", "JOY", "dual");
        }
        joy_run = 1;
        joy_blk_lim_log = 0;
        joy_spd_log = 0;
    } else if (!joy_any && joy_run) {
        DBG_INFO("JOG", "JOY", "idle");
        joy_run = 0;
        joy_blk_lim_log = 0;
        joy_spd_log = 0;
    }

    if (z_on && !was_z_on) {
        hand_reset_axis(AXIS_Z);
        joy_mpg_isolate_peer(AXIS_Z);
        DBG_INFO("JOG", "JOY", "axis Z");
    }
    if (x_on && !was_x_on) {
        hand_reset_axis(AXIS_X);
        joy_mpg_isolate_peer(AXIS_X);
        DBG_INFO("JOG", "JOY", "axis X");
    }

    if ((z_on && !was_z_on) || (x_on && !was_x_on)) {
        uint8_t single_feed = (uint8_t)((x_on && !z_on) || (z_on && !x_on));
        if (single_feed) {
            /* Синхрон target/cmd; не jog_release — иначе DECEL/bl_drain на peer */
            DBG_INFO("JOG", "JOY", "handoff halt");
            planner_jog_halt();
            mpg_sync_cmd();
        } else if (dds_motion_busy() || backlash_pending(AXIS_X) > 0 ||
                   backlash_pending(AXIS_Z) > 0) {
            DBG_INFO("JOG", "JOY", "restart stop");
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

    int32_t px = dds_get_position(AXIS_X);
    int32_t pz = dds_get_position(AXIS_Z);
    tx = x_on ? px : jog_peer_axis_hold(AXIS_X);
    tz = z_on ? pz : jog_peer_axis_hold(AXIS_Z);
    int32_t ox = tx;
    int32_t oz = tz;
    uint8_t need_exec = 0U;
    uint8_t clamped = 0U;

    if (z_on) {
        int32_t tgt_z = dds_get_target(AXIS_Z);
        int32_t rem_z = (z_sign > 0) ? (tgt_z - pz) : ((z_sign < 0) ? (pz - tgt_z) : 0);
        if (rem_z < JOY_RUNWAY_REFRESH) {
            int32_t base_z = (rem_z > 0) ? tgt_z : pz;
            int32_t raw_z = base_z + z_sign * JOY_RUNWAY_FILL;
            tz = jog_clamp_target(AXIS_Z, raw_z, btn.joy_rapid);
            if (!btn.joy_rapid && tz != raw_z) {
                clamped = 1U;
            }
            need_exec = 1U;
            DBG_VERBOSE_VAL_I32("JOG", "JOY", "rw Z", tz);
        } else {
            tz = tgt_z;
        }
    }
    if (x_on) {
        int32_t tgt_x = dds_get_target(AXIS_X);
        int32_t rem_x = (x_sign > 0) ? (tgt_x - px) : ((x_sign < 0) ? (px - tgt_x) : 0);
        if (rem_x < JOY_RUNWAY_REFRESH) {
            int32_t base_x = (rem_x > 0) ? tgt_x : px;
            int32_t raw_x = base_x + x_sign * JOY_RUNWAY_FILL;
            tx = jog_clamp_target(AXIS_X, raw_x, btn.joy_rapid);
            if (!btn.joy_rapid && tx != raw_x) {
                clamped = 1U;
            }
            need_exec = 1U;
            DBG_VERBOSE_VAL_I32("JOG", "JOY", "rw X", tx);
        } else {
            tx = tgt_x;
        }
    }

    {
        float spd_f = jog_speed_dual_mm_min(btn.joy_rapid);
        uint16_t spd_u = (uint16_t)spd_f;
        /* гистерезис ≥5 мм/мин — иначе шум пота спамит INFO каждый тик */
        uint16_t d = (spd_u > joy_spd_log) ? (uint16_t)(spd_u - joy_spd_log)
                                           : (uint16_t)(joy_spd_log - spd_u);
        if (d >= 5U) {
            DBG_INFO_VAL("JOG", "JOY", btn.joy_rapid ? "F rapid" : "F pot",
                         (uint32_t)spd_u);
            joy_spd_log = spd_u;
        }
    }

    /* Лимит/цель=позиция: cruise не гасим — только обновить скорость с пота */
    if (tx == ox && tz == oz) {
        if (!btn.joy_rapid && (clamped || need_exec) && !joy_blk_lim_log) {
            DBG_INFO("JOG", "JOY", "blk lim");
            joy_blk_lim_log = 1;
        }
        if (dds_motion_jog_cruise_active() && !mpg_active) {
            planner_exec_joy(tx, tz, jog_speed_dual_mm_min(btn.joy_rapid));
        }
        return;
    }
    joy_blk_lim_log = 0;

    if (!need_exec) {
        return;
    }

    if (clamped) {
        DBG_VERBOSE("JOG", "JOY", "clamp");
    }
    planner_exec_joy(tx, tz, jog_speed_dual_mm_min(btn.joy_rapid));
}

void motion_jog_poll(void) {  /* РГИ: batch тиков, dir_lock, idle stop */
    uint8_t axis;
    int32_t steps;
    uint8_t any_tick = 0U;
    uint8_t tick_n = 0U;
    uint8_t lim_hit = 0U;
    uint8_t sc_req;
    uint8_t sc_busy;
    SwitchState_t *sw = &mpg_sw;
    ButtonState_t *btn = &mpg_btn;
    static uint8_t mpg_blk_estop_log = 0;
    static uint8_t mpg_blk_off_log = 0;
    static uint8_t mpg_blk_golim_log = 0;
    static uint8_t mpg_blk_joy_log = 0;
    static uint8_t sc_blk_off_log = 0;
    static uint8_t sc_blk_golim_log = 0;

    *btn = ui_buttons_get_state();
    sc_req = 0U;
    sc_busy = 0U;
    if (!ui_menu_is_active()) {
        sc_busy = ui_buttons_set_coord_busy();  /* любая L/R/U/D — блок MPG */
        sc_req = ui_buttons_set_coord_id();     /* ровно одна — set-coord */
    }

    /*
     * Release только когда ВСЕ L/R/U/D отпущены.
     * id==0 при дребезге/двух кнопках — НЕ release (иначе MPG едет с пальцем на кнопке).
     */
    if (!sc_busy) {
        sc_blk_off_log = 0;
        sc_blk_golim_log = 0;
        if (sc_btn) {
            (void)set_coord_poll(0U);
        }
    }

    if (estop_is_triggered()) {
        if (!mpg_blk_estop_log) {
            DBG_INFO("JOG", "MPG", "blk estop");
            mpg_blk_estop_log = 1;
        }
        mpg_active = 0;
        mpg_rev_cnt = 0;
        mpg_dir_lock = 0;
        mpg_batch_ms = 0;
        sc_btn = 0;
        sc_dirty = 0;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
        mpg_rapid_prev = 0;
        mpg_approach_arm = 0;
        mpg_approach_ticks = 0;
        mpg_approach_go = 0;
#endif
        return;
    }
    mpg_blk_estop_log = 0;

    /* Любая L/R/U/D (в т.ч. дребезг) или сессия SC — MPG молчит */
    if (sc_busy || sc_btn) {
        if (ui_switches_mode_off()) {
            if (!sc_blk_off_log) {
                DBG_INFO("JOG", "SC", "blk MODE_OFF");
                sc_blk_off_log = 1;
            }
            sc_mpg_gate();
            return;
        }
        if (go_lim_active) {
            if (!sc_blk_golim_log) {
                DBG_INFO("JOG", "SC", "blk golim");
                sc_blk_golim_log = 1;
            }
            sc_mpg_gate();
            return;
        }
        if (sc_req) {
            *sw = ui_switches_get_state();
            if (set_coord_poll(sc_req)) {
                return;
            }
        }
        /* busy без id (дребезг/две) — держать сессию, тики не в MPG */
        sc_mpg_gate();
        return;
    }

    /* После release/zero — ждать тишину энкодера, не пускать MPG */
    if (sc_cooldown_active()) {
        return;
    }

    if (ui_switches_mode_off()) {
        if (!mpg_blk_off_log) {
            DBG_INFO("JOG", "MPG", "blk MODE_OFF");
            mpg_blk_off_log = 1;
        }
        mpg_active = 0;
        mpg_rev_cnt = 0;
        mpg_dir_lock = 0;
        mpg_batch_ms = 0;
        sc_btn = 0;
        sc_dirty = 0;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
        mpg_rapid_prev = 0;
        mpg_approach_arm = 0;
        mpg_approach_ticks = 0;
        mpg_approach_go = 0;
#endif
        return;
    }
    mpg_blk_off_log = 0;

    if (go_lim_active) {
        if (!mpg_blk_golim_log) {
            DBG_INFO("JOG", "MPG", "blk golim");
            mpg_blk_golim_log = 1;
        }
        return;
    }
    mpg_blk_golim_log = 0;

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
        DBG_INFO("JOG", "MPG", "startup sync");
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

    /* Любая подача джойстика — РГИ/coast не трогают оси (изоляция от joy) */
    if (joy_x_on || joy_z_on) {
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
        if (mpg_approach_go) {
            planner_jog_halt();
            mpg_approach_go = 0;
            DBG_INFO("JOG", "MPG", "approach abort joy");
        }
        if (mpg_approach_arm) {
            mpg_approach_arm = 0;
            mpg_approach_ticks = 0;
            mpg_cmd[mpg_axis_last] = dds_get_position(mpg_axis_last);
        }
#endif
        if (mpg_active) {
            mpg_active = 0;
            mpg_batch_ms = 0;
            mpg_dir_lock = 0;
            mpg_rev_cnt = 0;
        }
        if (!mpg_blk_joy_log) {
            DBG_INFO("JOG", "MPG", "blk joy");
            mpg_blk_joy_log = 1;
        }
        ui_encoder_discard_mpg_delta();
        return;
    }
    mpg_blk_joy_log = 0;

    if (ui_encoder_peek_mpg_delta() == 0) {
        if (mpg_rapid_deferred(btn)) {
            return;
        }
        if (mpg_batch_ms != 0UL && (millis() - mpg_batch_ms) >= MPG_BATCH_MS) {
            mpg_batch_ms = 0UL;
            DBG_VERBOSE("JOG", "MPG", "batch commit");
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
                    if (!mpg_coast_logged) {
                        DBG_VERBOSE("JOG", "MPG", "coast");
                        mpg_coast_logged = 1U;
                    }
                    mpg_planner_commit(mpg_axis_last, sw, btn, 0U, 0U);
                }
                return;
            }
            /* ТЗ: idle → stop на pos; halt — без jog_release/DECEL (рывок после паузы) */
            if (backlash_pending(mpg_axis_last) > 0) {
                return;
            }
            if (dds_motion_jog_cruise_active() || dds_motion_busy()) {
                planner_jog_halt();
            }
            mpg_cmd[mpg_axis_last] = dds_get_position(mpg_axis_last);
            mpg_active = 0;
            mpg_rev_cnt = 0;
            mpg_batch_ms = 0;
            mpg_coast_logged = 0;
            mpg_idle_catch_logged = 0;
            mpg_hold_tgt_logged = 0;
            mpg_cruise_keep_logged = 0;
            mpg_lead_ok = 0;
            ui_encoder_discard_mpg_delta();
            DBG_INFO_VAL("JOG", "MPG", "idle stop", (uint32_t)mpg_axis_last);
        }
        return;
    }

    axis = sw->mpg_axis;

    /* любая активность энкодера продлевает сессию (в т.ч. игнор реверса) */
    mpg_last_ms = millis();
    if (dds_motion_jog_cruise_active()) {
        mpg_active = 1;
    }

    {
        int8_t lock_sign = mpg_dir_lock;
        uint8_t preview = mpg_rapid_deferred(btn);

        /* Pos обогнал cmd — sync до тиков, иначе первый тик съедается в commit */
        if (lock_sign != 0) {
            int32_t p = dds_get_position(axis);
            if (lock_sign > 0 && p > mpg_cmd[axis]) {
                mpg_cmd[axis] = p;
            } else if (lock_sign < 0 && p < mpg_cmd[axis]) {
                mpg_cmd[axis] = p;
            }
        }

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
                    tick_n++;
#if MPG_RAPID_MODE == MPG_RAPID_MODE_APPROACH
                    if (!mpg_approach_ticks) {
                        DBG_INFO("JOG", "MPG", "arm tick");
                    }
                    mpg_approach_ticks = 1U;
#endif
                }
                continue;
            }

            if (lock_sign == 0) {
                lock_sign = tick_sign;
                mpg_dir_lock = tick_sign;
                mpg_rev_cnt = 0;
                DBG_INFO_VAL("JOG", "MPG", "dir",
                             (uint32_t)((int32_t)tick_sign + 2));
            } else if (tick_sign != lock_sign) {
                (void)ui_encoder_consume_mpg_tick();
                /*
                 * ТЗ: ≤N игнор, >N полный стоп — иначе «всегда одна сторона».
                 */
                if (mpg_active || dds_motion_jog_cruise_active()) {
                    mpg_rev_cnt++;
                    if (mpg_rev_cnt <= MPG_REV_IGNORE_N) {
                        DBG_VERBOSE_VAL("JOG", "MPG", "rev ign",
                                        (uint32_t)mpg_rev_cnt);
                        continue;
                    }
                    /* ТЗ: полный стоп сразу — не jog_release (докат в старом DIR) */
                    if (dds_motion_jog_cruise_active() || dds_motion_busy()) {
                        planner_jog_halt();
                    }
                    mpg_cmd[axis] = dds_get_position(axis);
                    mpg_active = 0;
                    mpg_dir_lock = 0;
                    mpg_rev_cnt = 0;
                    mpg_batch_ms = 0;
                    mpg_lead_ok = 0;
                    ui_encoder_discard_mpg_delta();
                    DBG_INFO("JOG", "MPG", "rev stop");
                    return;
                }
                mpg_rev_cnt++;
                if (mpg_rev_cnt <= MPG_REV_IGNORE_N) {
                    DBG_VERBOSE_VAL("JOG", "MPG", "rev ign",
                                    (uint32_t)mpg_rev_cnt);
                    continue;
                }
                lock_sign = tick_sign;
                mpg_dir_lock = tick_sign;
                mpg_rev_cnt = 0;
                DBG_INFO_VAL("JOG", "MPG", "dir flip",
                             (uint32_t)((int32_t)tick_sign + 2));
                steps = jog_steps_from_delta(axis, tick_sign, sw->mpg_scale,
                                             mpg_step_use_rapid(btn));
                mpg_apply_tick(axis, steps, lim_bypass, &lim_hit);
                if (steps != 0) {
                    any_tick = 1U;
                    tick_n++;
                }
                continue;
            } else {
                mpg_rev_cnt = 0;
            }

            steps = jog_steps_from_delta(axis, tick_sign, sw->mpg_scale,
                                         mpg_step_use_rapid(btn));
            (void)ui_encoder_consume_mpg_tick();
            mpg_apply_tick(axis, steps, lim_bypass, &lim_hit);
            if (steps != 0) {
                any_tick = 1U;
                tick_n++;
            }
        }
    }

    if (!any_tick) return;

    mpg_active = 1;
    mpg_last_ms = millis();
    mpg_coast_logged = 0;       /* новый пакет тиков — coast/catch/hold снова можно */
    mpg_idle_catch_logged = 0;
    mpg_hold_tgt_logged = 0;
    mpg_cruise_keep_logged = 0;
    if (mpg_batch_ms == 0UL) {
        mpg_batch_ms = mpg_last_ms;
    }

    mpg_log_cmd_hand(axis, tick_n);

    if (mpg_rapid_deferred(btn)) {
        DBG_VERBOSE_VAL("JOG", "MPG", "defer", (uint32_t)tick_n);
        return;
    }

    if (ui_encoder_peek_mpg_delta() != 0 &&
        (millis() - mpg_batch_ms) < MPG_BATCH_MS) {
        return;
    }
    mpg_batch_ms = 0UL;

    mpg_planner_commit(axis, sw, btn, lim_hit, 1U);
}
