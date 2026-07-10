#include "stepper_gen.h"
#include "../hal/hal_pins.h"
#include "backlash.h"
#include "limits.h"
#include "../debug/debug_serial.h"
#include "../../config/config.h"
#include "../../config/config_machine.h"
#include <avr/interrupt.h>
#include <string.h>

/* Состояние осей X и Z */
static AxisState_t axis_x;
static AxisState_t axis_z;

/* Профиль трапецеидального разгона/торможения (master-ось задаёт темп) */
typedef struct {
    uint8_t active;              /* 1 — движение выполняется */
    uint8_t flags;               /* копия MotionCommand_t.flags */
    uint8_t jog_cruise;          /* 1 — jog без финального стопа */
    uint8_t master_axis;         /* ось, по шагам которой считается профиль */
    uint32_t step_count;         /* всего step_events до завершения */
    uint32_t step_events;        /* счётчик шагов master-оси */
    uint32_t accelerate_until;   /* step_events до конца разгона */
    uint32_t decelerate_after;   /* step_events после начала торможения */
    uint32_t current_rate;       /* текущая скорость профиля, steps/s */
    uint32_t nominal_rate;       /* крейсерская скорость, steps/s */
    uint32_t initial_rate;       /* скорость старта, steps/s */
    uint32_t final_rate;         /* скорость финиша, steps/s */
    uint32_t acceleration;       /* ускорение, steps/s² */
    uint32_t axis_steps[2];      /* длина сегмента по X и Z, шаги */
} MotionProfile_t;

static MotionProfile_t motion_prof;

/* scratch для dds_motion_start — экономия ~40 байт стека (SRAM ~50%) */
static struct {
    int32_t cx;
    int32_t cz;
    int32_t tx;
    int32_t tz;
    uint32_t ux;
    uint32_t uz;
    uint32_t um;
    uint8_t master;
    uint32_t nominal;
    uint32_t entry;
    uint32_t exit_r;
    uint32_t accel;
} dds_start;

/* Кольцевая очередь отладочного лога (ISR → main) */
#define MOTION_LOG_QSIZE 6U
#define MOTION_LOG_CRUISE 1U   /* вход в крейсер */
#define MOTION_LOG_DECEL  2U   /* начало торможения */
#define MOTION_LOG_DIR    3U   /* смена направления */

typedef struct {
    uint8_t type;       /* MOTION_LOG_* */
    uint8_t axis;
    int8_t dir;         /* +1 / -1 */
    uint8_t pad;
    uint32_t step_ev;
    uint32_t rate;
    uint32_t nominal;
    uint32_t dec_after;
    uint32_t step_cnt;
    int32_t pos;
    int32_t tgt;
} MotionLogItem_t;

static volatile uint8_t mlog_w;  /* индекс записи (ISR) */
static volatile uint8_t mlog_r;  /* индекс чтения (main) */
static MotionLogItem_t mlog_q[MOTION_LOG_QSIZE];

/* feed_accel × scale → мм/с² */
#define ACCEL_MM_S2_SCALE 50.0f

/* steps/sec → DDS-приращение: (sps << 31) / STEP_ISR_FREQ_HZ */
uint32_t dds_calc_increment(uint32_t steps_per_sec) {
    if (steps_per_sec < 1U) steps_per_sec = 1U;
    return (uint32_t)(((uint64_t)steps_per_sec << 31) / STEP_ISR_FREQ_HZ);
}

/* Целочисленный квадратный корень (для расчёта пиковой скорости короткого сегмента) */
static uint32_t isqrt32(uint32_t x) {
    uint32_t op = x;
    uint32_t res = 0;
    uint32_t one = 1UL << 30;

    while (one > op) {
        one >>= 2;
    }
    while (one != 0U) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }
    return res;
}

/* мм/мин → steps/s с нижним порогом 1 sps */
static uint32_t mm_min_to_sps(uint8_t axis, float mm_min) {
    if (mm_min < 0.25f) return 1U;
    if (mm_min < 1.0f) mm_min = 1.0f;
    return config_mm_min_to_sps(axis, mm_min);
}

/* Ускорение оси из config feed_accel, steps/s² */
static uint32_t axis_accel_steps_s2(uint8_t axis) {
    uint8_t fa = config_get_feed_accel(axis);
    float accel_mm_s2 = (float)fa * ACCEL_MM_S2_SCALE;
    float spm = config_get_steps_per_mm(axis);
    uint32_t a = (uint32_t)(accel_mm_s2 * spm + 0.5f);
    if (a < 1U) a = 1U;
    return a;
}

/* Дистанция разгона/торможения: (v_hi² - v_lo²) / (2·a) в шагах master-оси */
static uint32_t accel_distance(uint32_t rate_hi, uint32_t rate_lo, uint32_t accel) {
    if (accel == 0U || rate_hi <= rate_lo) return 0U;
    uint64_t diff = (uint64_t)rate_hi * (uint64_t)rate_hi - (uint64_t)rate_lo * (uint64_t)rate_lo;
    return (uint32_t)(diff / ((uint64_t)accel * 2ULL));
}

/* Знаменатель для пропорции скоростей X/Z (max оси или step_count) */
static uint32_t motion_profile_denom(void) {
    MotionProfile_t *p = &motion_prof;

    if (p->jog_cruise) {
        uint32_t d = p->axis_steps[AXIS_X] > p->axis_steps[AXIS_Z] ?
                     p->axis_steps[AXIS_X] : p->axis_steps[AXIS_Z];
        return (d < 1U) ? 1U : d;
    }
    return p->step_count;
}

static void motion_update_dirs(int32_t dx, int32_t dz);

/* Фаза профиля: 0 разгон, 1 крейсер, 2 торможение, 0xFF неактивен */
static uint8_t motion_profile_phase(const MotionProfile_t *p) {
    if (!p->active) return 0xFFU;
    if (p->jog_cruise && p->decelerate_after >= 0xFFFFFF00UL) {
        if (p->step_events <= p->accelerate_until) return 0U;
        return 1U;
    }
    if (p->step_events <= p->accelerate_until) return 0U;
    if (p->step_events > p->decelerate_after) return 2U;
    return 1U;
}

static void mlog_reset(void) {  /* очистка очереди лога */
    uint8_t sreg = SREG;
    cli();
    mlog_w = 0U;
    mlog_r = 0U;
    SREG = sreg;
}

static void mlog_push_isr(const MotionLogItem_t *item) {  /* запись из ISR */
    uint8_t sreg = SREG;
    uint8_t next;

    cli();
    next = (uint8_t)((mlog_w + 1U) % MOTION_LOG_QSIZE);
    if (next != mlog_r) {
        mlog_q[mlog_w] = *item;
        mlog_w = next;
    }
    SREG = sreg;
}

static void mlog_push_dir(uint8_t axis, int8_t dir, int32_t pos, int32_t tgt) {
    MotionLogItem_t item;

    memset(&item, 0, sizeof(item));
    item.type = MOTION_LOG_DIR;
    item.axis = axis;
    item.dir = dir;
    item.pos = pos;
    item.tgt = tgt;
    mlog_push_isr(&item);
}

static void mlog_push_cruise(const MotionProfile_t *p) {
    MotionLogItem_t item;
    int32_t pos;

    memset(&item, 0, sizeof(item));
    item.type = MOTION_LOG_CRUISE;
    item.axis = p->master_axis;
    item.step_ev = p->step_events;
    item.rate = p->current_rate;
    item.nominal = p->nominal_rate;
    pos = dds_get_position(p->master_axis);
    item.pos = pos;
    mlog_push_isr(&item);
}

static void mlog_push_decel(const MotionProfile_t *p) {
    MotionLogItem_t item;
    int32_t pos;

    memset(&item, 0, sizeof(item));
    item.type = MOTION_LOG_DECEL;
    item.axis = p->master_axis;
    item.step_ev = p->step_events;
    item.rate = p->current_rate;
    item.dec_after = p->decelerate_after;
    item.step_cnt = p->step_count;
    pos = dds_get_position(p->master_axis);
    item.pos = pos;
    mlog_push_isr(&item);
}

void dds_motion_log_poll(void) {  /* main: вывод записей лога в DBG */
    while (mlog_r != mlog_w) {
        MotionLogItem_t item;
        uint8_t sreg = SREG;

        cli();
        if (mlog_r == mlog_w) {
            SREG = sreg;
            break;
        }
        item = mlog_q[mlog_r];
        mlog_r = (uint8_t)((mlog_r + 1U) % MOTION_LOG_QSIZE);
        SREG = sreg;

        switch (item.type) {
            case MOTION_LOG_DIR:
                DBG_PLN_DIR(item.axis, item.dir, item.pos, item.tgt);
                break;
            case MOTION_LOG_CRUISE:
                DBG_PLN_CRUISE(item.axis, item.step_ev, item.rate, item.nominal, item.pos);
                break;
            case MOTION_LOG_DECEL:
                DBG_PLN_DECEL(item.axis, item.step_ev, item.rate,
                              item.dec_after, item.step_cnt, item.pos);
                break;
            default:
                break;
        }
    }
}

/* Ограничить step_increment при активной выборке люфта */
static void motion_boost_backlash_rates(void) {
    uint8_t axis;
    AxisState_t *axes[2] = {&axis_x, &axis_z};
    uint32_t min_inc;
    uint32_t max_inc;

    for (axis = 0; axis < 2U; axis++) {
        if (backlash_pending(axis) <= 0) continue;

        min_inc = dds_calc_increment(
            config_mm_min_to_sps(axis, (float)config_backlash_get_min_speed()));
        max_inc = dds_calc_increment(
            config_mm_min_to_sps(axis, (float)config_backlash_get_auto_speed()));

        if (axes[axis]->step_increment < min_inc) {
            axes[axis]->step_increment = min_inc;
        }
        if (axes[axis]->step_increment > max_inc) {
            axes[axis]->step_increment = max_inc;
        }
    }
}

/* Распределить current_rate по осям пропорционально axis_steps */
static void motion_apply_rates(void) {
    MotionProfile_t *p = &motion_prof;
    uint32_t r = p->current_rate;
    uint32_t denom;

    denom = motion_profile_denom();
    if (denom == 0U) return;

    if (p->axis_steps[AXIS_X] > 0U) {
        uint32_t sx = (uint32_t)(((uint64_t)r * p->axis_steps[AXIS_X]) / denom);
        if (sx < 1U) sx = 1U;
        axis_x.step_increment = dds_calc_increment(sx);
    } else {
        axis_x.step_increment = 0U;
    }
    if (p->axis_steps[AXIS_Z] > 0U) {
        uint32_t sz = (uint32_t)(((uint64_t)r * p->axis_steps[AXIS_Z]) / denom);
        if (sz < 1U) sz = 1U;
        axis_z.step_increment = dds_calc_increment(sz);
    } else {
        axis_z.step_increment = 0U;
    }
    motion_boost_backlash_rates();
}

/* Расчёт точек разгона/торможения и стартовых rate обеих осей */
static void motion_prep_profile(uint32_t steps, uint32_t nominal, uint32_t initial,
                                uint32_t final, uint32_t accel) {
    MotionProfile_t *p = &motion_prof;

    if (steps < 1U) steps = 1U;
    if (nominal < 1U) nominal = 1U;
    if (initial < 1U) initial = 1U;
    if (final < 1U) final = 1U;
    if (accel < 1U) accel = 1U;

    if (initial > nominal) initial = nominal;
    if (final > nominal) final = nominal;

    uint32_t accel_dist = accel_distance(nominal, initial, accel);
    uint32_t decel_dist = accel_distance(nominal, final, accel);

    if (accel_dist + decel_dist >= steps) {
        uint32_t peak_sq = (uint32_t)((uint64_t)initial * initial +
                                      (uint64_t)accel * steps);
        uint32_t peak = isqrt32(peak_sq);
        if (peak < initial) peak = initial;
        if (peak > nominal) peak = nominal;
        nominal = peak;
        p->accelerate_until = steps >> 1;
        p->decelerate_after = steps >> 1;
    } else {
        p->accelerate_until = accel_dist;
        p->decelerate_after = steps - decel_dist;
    }

    p->step_count = steps;
    p->step_events = 0U;
    p->nominal_rate = nominal;
    p->initial_rate = initial;
    p->final_rate = final;
    p->current_rate = initial;
    p->acceleration = accel;
    motion_apply_rates();
}

/* Один шаг master-оси: обновление current_rate и лог фаз */
static void motion_profile_step(uint8_t axis) {
    MotionProfile_t *p = &motion_prof;
    uint8_t phase_before;
    uint8_t phase_after;

    if (!p->active || axis != p->master_axis) return;

    if (p->current_rate < 1U) p->current_rate = 1U;

    phase_before = motion_profile_phase(p);

    p->step_events++;

    if (p->step_events <= p->accelerate_until) {
        uint32_t inc = (p->acceleration << 14) / p->current_rate;
        if (inc < 1U) inc = 1U;
        p->current_rate += inc;
        if (p->current_rate > p->nominal_rate) {
            p->current_rate = p->nominal_rate;
        }
    } else if (p->step_events > p->decelerate_after) {
        uint32_t dec = (p->acceleration << 14) / p->current_rate;
        if (dec < 1U) dec = 1U;
        if (p->current_rate > dec + p->final_rate) {
            p->current_rate -= dec;
        } else {
            p->current_rate = p->final_rate;
        }
    }
    motion_apply_rates();

    phase_after = motion_profile_phase(p);
    if (phase_before == 0U && phase_after >= 1U) {
        mlog_push_cruise(p);
    }
    if (phase_before < 2U && phase_after == 2U) {
        mlog_push_decel(p);
    }
}

/* 1 если профиль завершён (не jog_cruise, цели и люфт достигнуты) */
static uint8_t motion_profile_done(void) {
    MotionProfile_t *p = &motion_prof;

    if (!p->active) return 1U;
    if (p->jog_cruise) return 0U;
    if (p->step_events < p->step_count) return 0U;
    if (backlash_pending(AXIS_X) > 0 || backlash_pending(AXIS_Z) > 0) return 0U;
    if (!dds_at_target(AXIS_X) || !dds_at_target(AXIS_Z)) return 0U;
    return 1U;
}

void dds_init(void) {  /* сброс осей, профиля, очереди лога */
    memset(&axis_x, 0, sizeof(AxisState_t));
    memset(&axis_z, 0, sizeof(AxisState_t));
    memset(&motion_prof, 0, sizeof(MotionProfile_t));
    mlog_reset();
}

static void dir_x_set(uint8_t d) {  /* DIR X с учётом инверсии */
    if (config_get_dir_invert(AXIS_X)) d = !d;
    DIR_X_SET(d);
}

static void dir_z_set(uint8_t d) {  /* DIR Z с учётом инверсии */
    if (config_get_dir_invert(AXIS_Z)) d = !d;
    DIR_Z_SET(d);
}

void dds_set_speed(uint8_t axis, uint32_t steps_per_sec) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->step_increment = dds_calc_increment(steps_per_sec);
}

void dds_set_direction(uint8_t axis, uint8_t dir) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->direction = dir;
    if (axis == AXIS_X) {
        dir_x_set(dir);
    } else {
        dir_z_set(dir);
    }
}

void dds_enable(uint8_t axis, uint8_t enable) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    a->enabled = enable;
}

static void step_x_on(void) { STEP_X_ON(); }
static void step_x_off(void) { STEP_X_OFF(); }
static void step_z_on(void) { STEP_Z_ON(); }
static void step_z_off(void) { STEP_Z_OFF(); }

/* DDS-шаг одной оси: направление, люфт, импульс STEP, шаг профиля */
static void dds_axis_step(uint8_t axis, AxisState_t *a, void (*step_on)(void), void (*step_off)(void),
                          void (*dir_set)(uint8_t)) {
    uint8_t fwd;

    if (!a->enabled || a->step_increment == 0U) return;

    fwd = a->direction;

    if (motion_prof.jog_cruise) {
        if (a->position == a->target_position && backlash_pending(axis) <= 0) {
            return;
        }
        if (a->position != a->target_position) {
            fwd = (a->target_position > a->position) ? 1U : 0U;
            if (a->direction != fwd) {
                a->direction = fwd;
                dir_set(fwd);
                backlash_arm_axis(axis, fwd, 1);
                mlog_push_dir(axis, fwd ? 1 : -1, a->position, a->target_position);
            } else {
                dir_set(fwd);
            }
        } else {
            dir_set(fwd);
        }
    } else if (a->position == a->target_position && backlash_pending(axis) <= 0) {
        return;
    } else if (a->position != a->target_position) {
        fwd = (a->target_position > a->position) ? 1U : 0U;
        if (a->direction != fwd) {
            a->direction = fwd;
            dir_set(fwd);
            backlash_arm_axis(axis, fwd, 1);
            mlog_push_dir(axis, fwd ? 1 : -1, a->position, a->target_position);
        } else {
            dir_set(fwd);
        }
    } else {
        dir_set(fwd);
    }

    a->accumulator += a->step_increment;
    if (a->accumulator >= 0x80000000UL) {
        a->accumulator -= 0x80000000UL;
        step_on();
        if (!backlash_consume_step(axis, fwd)) {
            a->position += fwd ? 1 : -1;
        }
        step_off();
        motion_profile_step(axis);
    }
}

void stepper_generate_steps(void) {  /* ISR: шаг X, шаг Z, завершение профиля */
    dds_axis_step(AXIS_X, &axis_x, step_x_on, step_x_off, dir_x_set);
    dds_axis_step(AXIS_Z, &axis_z, step_z_on, step_z_off, dir_z_set);

    if (motion_prof.active && motion_prof.jog_cruise) {
        if (axis_x.position == axis_x.target_position &&
            axis_z.position == axis_z.target_position &&
            backlash_pending(AXIS_X) <= 0 &&
            backlash_pending(AXIS_Z) <= 0) {
            motion_prof.active = 0U;
            motion_prof.jog_cruise = 0U;
            axis_x.enabled = 0U;
            axis_z.enabled = 0U;
            axis_x.step_increment = 0U;
            axis_z.step_increment = 0U;
            return;
        }
    }

    if (motion_prof.active && motion_profile_done()) {
        motion_prof.active = 0U;
        axis_x.enabled = 0U;
        axis_z.enabled = 0U;
    }
}

int32_t dds_get_position(uint8_t axis) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    if (!(SREG & (uint8_t)(1U << 7))) {
        return a->position;
    }
    {
        uint8_t sreg = SREG;
        int32_t v;

        cli();
        v = a->position;
        SREG = sreg;
        return v;
    }
}

void dds_set_position(uint8_t axis, int32_t pos) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    if (!(SREG & (uint8_t)(1U << 7))) {
        a->position = pos;
        a->target_position = pos;
        return;
    }
    {
        uint8_t sreg = SREG;

        cli();
        a->position = pos;
        a->target_position = pos;
        SREG = sreg;
    }
}

void dds_set_target(uint8_t axis, int32_t target) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    if (!(SREG & (uint8_t)(1U << 7))) {
        a->target_position = target;
        return;
    }
    {
        uint8_t sreg = SREG;

        cli();
        a->target_position = target;
        SREG = sreg;
    }
}

int32_t dds_get_target(uint8_t axis) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    if (!(SREG & (uint8_t)(1U << 7))) {
        return a->target_position;
    }
    {
        uint8_t sreg = SREG;
        int32_t v;

        cli();
        v = a->target_position;
        SREG = sreg;
        return v;
    }
}

uint8_t dds_at_target(uint8_t axis) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    if (backlash_pending(axis) > 0) return 0U;
    return a->position == a->target_position;
}

uint8_t dds_get_direction(uint8_t axis) {
    AxisState_t *a = (axis == AXIS_X) ? &axis_x : &axis_z;
    return a->direction;
}

void dds_reset_accumulator(void) {
    axis_x.accumulator = 0U;
    axis_z.accumulator = 0U;
}

void dds_motion_start(const MotionCommand_t *cmd) {  /* backlash / jog cruise / сегмент */
    uint8_t sreg;

    if (!cmd) return;

    sreg = SREG;
    cli();

    dds_start.cx = axis_x.position;
    dds_start.cz = axis_z.position;
    dds_start.tx = cmd->target_x;
    dds_start.tz = cmd->target_z;

    motion_prof.active = 0U;
    motion_prof.flags = cmd->flags;
    motion_prof.jog_cruise = 0U;
    motion_prof.axis_steps[AXIS_X] = 0U;
    motion_prof.axis_steps[AXIS_Z] = 0U;

    if (cmd->flags & MOTION_FLAG_BACKLASH) {
        uint8_t ax = cmd->bl_axis;
        uint8_t ref = (ax == AXIS_X) ? BACKLASH_REF_DIR_X : BACKLASH_REF_DIR_Z;
        int32_t steps = cmd->bl_steps;

        if (steps <= 0) goto dds_motion_start_exit;

        backlash_queue_takeup(ax, ref, steps);
        dds_set_direction(ax, ref);
        dds_set_target(ax, dds_get_position(ax));
        dds_enable(ax, 1U);
        if (ax == AXIS_X) {
            motion_prof.axis_steps[AXIS_X] = (uint32_t)steps;
            dds_start.master = AXIS_X;
        } else {
            motion_prof.axis_steps[AXIS_Z] = (uint32_t)steps;
            dds_start.master = AXIS_Z;
        }
        dds_start.um = (uint32_t)steps;

        {
            float cap = (float)config_get_max_speed_mm_min(dds_start.master);
            float spd = cmd->nominal_mm_min;
            if (spd > cap) spd = cap;
            dds_start.nominal = mm_min_to_sps(dds_start.master, spd);
            dds_start.entry = mm_min_to_sps(dds_start.master, cmd->entry_mm_min);
            dds_start.exit_r = mm_min_to_sps(dds_start.master, cmd->exit_mm_min);
        }
        dds_start.accel = axis_accel_steps_s2(dds_start.master);

        motion_prof.master_axis = dds_start.master;
        motion_prof.active = 1U;
        motion_prep_profile(dds_start.um, dds_start.nominal, dds_start.entry, dds_start.exit_r,
                            dds_start.accel);
        goto dds_motion_start_exit;
    }

    {
        int32_t dx = dds_start.tx - dds_start.cx;
        int32_t dz = dds_start.tz - dds_start.cz;
        dds_start.ux = (uint32_t)((dx < 0) ? -dx : dx);
        dds_start.uz = (uint32_t)((dz < 0) ? -dz : dz);
        dds_start.um = dds_start.ux > dds_start.uz ? dds_start.ux : dds_start.uz;

        if (dds_start.um == 0U) goto dds_motion_start_exit;

        if (cmd->flags & MOTION_FLAG_JOG_CRUISE) {
            uint32_t entry_sps;
            uint32_t accel_dist;

            motion_update_dirs(dx, dz);
            axis_x.target_position = dds_start.tx;
            axis_z.target_position = dds_start.tz;
            if (dds_start.ux > 0U) dds_enable(AXIS_X, 1U);
            if (dds_start.uz > 0U) dds_enable(AXIS_Z, 1U);

            dds_start.master = (dds_start.ux >= dds_start.uz) ? AXIS_X : AXIS_Z;
            {
                float cap = (float)config_get_max_speed_mm_min(dds_start.master);
                float spd = cmd->nominal_mm_min;
                if (spd > cap) spd = cap;
                dds_start.nominal = mm_min_to_sps(dds_start.master, spd);
                entry_sps = mm_min_to_sps(dds_start.master, cmd->entry_mm_min);
            }
            if (entry_sps < 1U) entry_sps = 1U;
            dds_start.accel = axis_accel_steps_s2(dds_start.master);

            motion_prof.jog_cruise = 1U;
            motion_prof.axis_steps[AXIS_X] = dds_start.ux;
            motion_prof.axis_steps[AXIS_Z] = dds_start.uz;
            motion_prof.master_axis = dds_start.master;
            motion_prof.acceleration = dds_start.accel;
            motion_prof.nominal_rate = dds_start.nominal;
            motion_prof.initial_rate = entry_sps;
            motion_prof.current_rate = entry_sps;
            motion_prof.final_rate = 1U;
            motion_prof.step_events = 0U;
            motion_prof.step_count = 0xFFFFFFFFU;
            motion_prof.decelerate_after = 0xFFFFFFFEU;
            accel_dist = accel_distance(dds_start.nominal, entry_sps, dds_start.accel);
            if (accel_dist < 1U) accel_dist = 1U;
            motion_prof.accelerate_until = accel_dist;
            motion_prof.active = 1U;
            motion_apply_rates();
            goto dds_motion_start_exit;
        }

        if (dx != 0) {
            uint8_t nd = (dx > 0) ? 1U : 0U;
            if (axis_x.direction != nd) {
                mlog_push_dir(AXIS_X, nd ? 1 : -1, axis_x.position, dds_start.tx);
            }
            backlash_arm_axis(AXIS_X, nd, 1);
            dds_set_direction(AXIS_X, nd);
        }
        if (dz != 0) {
            uint8_t nd = (dz > 0) ? 1U : 0U;
            if (axis_z.direction != nd) {
                mlog_push_dir(AXIS_Z, nd ? 1 : -1, axis_z.position, dds_start.tz);
            }
            backlash_arm_axis(AXIS_Z, nd, 1);
            dds_set_direction(AXIS_Z, nd);
        }

        axis_x.target_position = dds_start.tx;
        axis_z.target_position = dds_start.tz;

        motion_prof.axis_steps[AXIS_X] = dds_start.ux;
        motion_prof.axis_steps[AXIS_Z] = dds_start.uz;
        dds_start.master = (dds_start.ux >= dds_start.uz) ? AXIS_X : AXIS_Z;

        {
            uint32_t bl_extra = (uint32_t)backlash_pending(dds_start.master);
            if (bl_extra > 0U) {
                dds_start.um += bl_extra;
            }
        }

        {
            float cap = (float)config_get_max_speed_mm_min(dds_start.master);
            float spd = cmd->nominal_mm_min;
            if (spd > cap) spd = cap;
            dds_start.nominal = mm_min_to_sps(dds_start.master, spd);
            dds_start.entry = mm_min_to_sps(dds_start.master, cmd->entry_mm_min);
            dds_start.exit_r = mm_min_to_sps(dds_start.master, cmd->exit_mm_min);
        }
        dds_start.accel = axis_accel_steps_s2(dds_start.master);

        if (dds_start.ux > 0U) dds_enable(AXIS_X, 1U);
        if (dds_start.uz > 0U) dds_enable(AXIS_Z, 1U);

        motion_prof.master_axis = dds_start.master;
        motion_prof.active = 1U;
        motion_prep_profile(dds_start.um, dds_start.nominal, dds_start.entry, dds_start.exit_r,
                            dds_start.accel);
    }

dds_motion_start_exit:
    SREG = sreg;
}

/* Установить DIR и arm люфта при смене знака смещения (jog retarget) */
static void motion_update_dirs(int32_t dx, int32_t dz) {
    if (dx != 0) {
        uint8_t nd = (dx > 0) ? 1U : 0U;
        if (axis_x.direction != nd) {
            mlog_push_dir(AXIS_X, nd ? 1 : -1, axis_x.position, axis_x.position + dx);
        }
        backlash_arm_axis(AXIS_X, nd, 1);
        dds_set_direction(AXIS_X, nd);
    }
    if (dz != 0) {
        uint8_t nd = (dz > 0) ? 1U : 0U;
        if (axis_z.direction != nd) {
            mlog_push_dir(AXIS_Z, nd ? 1 : -1, axis_z.position, axis_z.position + dz);
        }
        backlash_arm_axis(AXIS_Z, nd, 1);
        dds_set_direction(AXIS_Z, nd);
    }
}

/* Продлить профиль jog при новой цели (пересчёт decelerate_after) */
static void motion_profile_extend(uint32_t remain_um, uint32_t ux, uint32_t uz) {
    MotionProfile_t *p = &motion_prof;
    uint32_t done = p->step_events;
    uint32_t decel_dist;

    if (remain_um < 1U) return;

    p->step_count = done + remain_um;
    p->axis_steps[AXIS_X] = ux;
    p->axis_steps[AXIS_Z] = uz;
    p->master_axis = (ux >= uz) ? AXIS_X : AXIS_Z;
    p->accelerate_until = done;

    decel_dist = accel_distance(p->current_rate, p->final_rate, p->acceleration);
    if (decel_dist >= remain_um) {
        p->decelerate_after = done;
    } else {
        p->decelerate_after = done + remain_um - decel_dist;
    }
    motion_apply_rates();
}

uint8_t dds_motion_jog_retarget(const MotionCommand_t *cmd) {  /* смена цели jog на лету */
    uint8_t sreg;
    uint8_t rc = 0U;
    int32_t cx;
    int32_t cz;
    int32_t dx;
    int32_t dz;
    uint32_t ux;
    uint32_t uz;
    uint32_t um;

    if (!cmd || !(cmd->flags & MOTION_FLAG_JOG)) return 0U;
    if (!motion_prof.active || !(motion_prof.flags & MOTION_FLAG_JOG)) return 0U;
    if ((cmd->flags & MOTION_FLAG_JOG_CRUISE) && !motion_prof.jog_cruise) return 0U;

    sreg = SREG;
    cli();

    cx = dds_get_position(AXIS_X);
    cz = dds_get_position(AXIS_Z);
    dx = cmd->target_x - cx;
    dz = cmd->target_z - cz;
    ux = (uint32_t)((dx < 0) ? -dx : dx);
    uz = (uint32_t)((dz < 0) ? -dz : dz);
    um = ux > uz ? ux : uz;

    if (motion_prof.jog_cruise) {
        uint8_t new_master = (ux >= uz) ? AXIS_X : AXIS_Z;
        if (new_master != motion_prof.master_axis) {
            goto dds_retarget_exit;
        }
    }

    if (um == 0U) {
        if (motion_prof.jog_cruise) {
            motion_prof.active = 0U;
            motion_prof.jog_cruise = 0U;
            axis_x.enabled = 0U;
            axis_z.enabled = 0U;
            axis_x.step_increment = 0U;
            axis_z.step_increment = 0U;
        }
        rc = 1U;
        goto dds_retarget_exit;
    }

    motion_update_dirs(dx, dz);
    dds_set_target(AXIS_X, cmd->target_x);
    dds_set_target(AXIS_Z, cmd->target_z);
    if (motion_prof.jog_cruise) {
        if (ux > 0U) {
            motion_prof.axis_steps[AXIS_X] = ux;
            dds_enable(AXIS_X, 1U);
        } else {
            motion_prof.axis_steps[AXIS_X] = 0U;
            axis_x.enabled = 0U;
            axis_x.step_increment = 0U;
        }
        if (uz > 0U) {
            motion_prof.axis_steps[AXIS_Z] = uz;
            dds_enable(AXIS_Z, 1U);
        } else {
            motion_prof.axis_steps[AXIS_Z] = 0U;
            axis_z.enabled = 0U;
            axis_z.step_increment = 0U;
        }
    } else {
        if (ux > 0U) {
            dds_enable(AXIS_X, 1U);
        } else {
            axis_x.enabled = 0U;
            axis_x.step_increment = 0U;
        }
        if (uz > 0U) {
            dds_enable(AXIS_Z, 1U);
        } else {
            axis_z.enabled = 0U;
            axis_z.step_increment = 0U;
        }
    }

    if (motion_prof.jog_cruise) {
        uint8_t master = (ux >= uz) ? AXIS_X : AXIS_Z;
        float cap = (float)config_get_max_speed_mm_min(master);
        float spd = cmd->nominal_mm_min;
        uint32_t nominal;

        if (spd > cap) spd = cap;
        nominal = mm_min_to_sps(master, spd);
        motion_prof.nominal_rate = nominal;
        motion_prof.axis_steps[AXIS_X] = ux;
        motion_prof.axis_steps[AXIS_Z] = uz;
        motion_prof.master_axis = master;
        motion_apply_rates();
        rc = 1U;
        goto dds_retarget_exit;
    }

    motion_profile_extend(um, ux, uz);
    rc = 1U;

dds_retarget_exit:
    SREG = sreg;
    return rc;
}

void dds_motion_jog_release(void) {
    dds_motion_stop();
}

void dds_motion_update_target(uint8_t axis, int32_t target) {
    if (!motion_prof.active) return;
    dds_set_target(axis, target);  /* dds_set_target: cli/sei */
}

float dds_motion_get_speed_mm_min(uint8_t axis) {
    if (!motion_prof.active) return 0.0f;
    if (axis > AXIS_Z) axis = AXIS_Z;

    uint32_t rate = motion_prof.current_rate;
    uint32_t denom = motion_profile_denom();
    if (denom == 0U) return 0.0f;

    uint32_t axis_rate = (uint32_t)(((uint64_t)rate * motion_prof.axis_steps[axis]) /
                                    denom);
    float spm = config_get_steps_per_mm(axis);
    if (spm < 1.0f) return 0.0f;
    return (float)axis_rate * 60.0f / spm;
}

uint8_t dds_motion_jog_cruise_active(void) {
    return motion_prof.active && motion_prof.jog_cruise;
}

uint8_t dds_motion_busy(void) {
    return motion_prof.active;
}

void dds_motion_stop(void) {  /* сброс профиля и отключение шагов */
    uint8_t sreg = SREG;

    cli();
    motion_prof.active = 0U;
    motion_prof.jog_cruise = 0U;
    axis_x.enabled = 0U;
    axis_z.enabled = 0U;
    axis_x.step_increment = 0U;
    axis_z.step_increment = 0U;
    SREG = sreg;
}
