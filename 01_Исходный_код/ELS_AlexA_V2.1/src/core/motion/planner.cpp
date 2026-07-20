#include "planner.h"
#include "stepper_gen.h"
#include "backlash.h"
#include "limits.h"
#include "../../config/config.h"
#include "../../config/config_machine.h"
#include "../../config/config_backlash.h"
#include "../debug/debug_serial.h"
#include "../debug/debug_trace.h"
#include "../ui/ui_pot.h"
#include <avr/interrupt.h>
#include <math.h>
#include <string.h>
#include <stddef.h>

/* Кольцевой буфер сегментов */
static PlannerBlock_t block_buffer[BLOCK_BUFFER_SIZE];
static uint8_t block_head = 0;   /* индекс записи */
static uint8_t block_tail = 0;   /* индекс исполнения */
static uint8_t block_exec = 0;   /* 1 — сегмент передан в dds */
static uint8_t startup_pending = 0;  /* ждём завершения автовыборки люфта */

#define ACCEL_MM_S2_SCALE 50.0f  /* feed_accel → мм/с² */

static void planner_jog_kind_flags(uint8_t *flags, const char *kind) {
    if (kind && kind[0] == 'M' && kind[1] == 'P' && kind[2] == 'G' && kind[3] == '\0') {
        *flags |= MOTION_FLAG_MPG;
    }
}

static uint8_t prev_index(uint8_t idx) {
    return (uint8_t)((idx + BLOCK_BUFFER_SIZE - 1U) % BLOCK_BUFFER_SIZE);
}

static uint8_t next_index(uint8_t idx) {
    return (uint8_t)((idx + 1U) % BLOCK_BUFFER_SIZE);
}

/* Ограничить скорость config max_speed оси */
static float cap_speed_mm_min(uint8_t axis, float mm_min) {
    float cap = (float)config_get_max_speed_mm_min(axis);
    if (mm_min < 1.0f) mm_min = 1.0f;
    if (mm_min > cap) mm_min = cap;
    return mm_min;
}

static float axis_accel_mm_s2(uint8_t axis) {
    return (float)config_get_feed_accel(axis) * ACCEL_MM_S2_SCALE;
}

/* Макс. entry_speed при заданном exit и длине сегмента: v = sqrt(v_exit² + 2·a·s) */
static float speed_from_dist(float v_exit_mm_min, float dist_steps, uint8_t axis) {
    float spm = config_get_steps_per_mm(axis);
    float accel = axis_accel_mm_s2(axis);
    float dist_mm = (spm > 0.0f) ? (dist_steps / spm) : 0.0f;
    float v_exit = v_exit_mm_min / 60.0f;
    float v2 = v_exit * v_exit + 2.0f * accel * dist_mm;
    if (v2 < 0.0f) v2 = 0.0f;
    return sqrtf(v2) * 60.0f;
}

static int32_t planner_mm_to_steps(uint8_t axis, float mm) {  /* мм → шаги */
    float spm = config_get_steps_per_mm(axis);
    return (int32_t)(mm * spm + (mm >= 0.0f ? 0.5f : -0.5f));
}

/* distance, direction_*, bl_axis от текущей позиции до цели блока */
static void planner_fill_geometry_segment(PlannerBlock_t *b, int32_t cx, int32_t cz) {
    if (b->flags & PLANNER_FLAG_BACKLASH) {
        b->distance = (float)b->bl_steps;
        b->direction_x = 0;
        b->direction_z = 0;
        return;
    }

    int32_t tx = b->target_steps_x;
    int32_t tz = b->target_steps_z;

    int32_t dx = tx - cx;
    int32_t dz = tz - cz;
    uint32_t ux = (uint32_t)((dx < 0) ? -dx : dx);
    uint32_t uz = (uint32_t)((dz < 0) ? -dz : dz);
    uint32_t um = ux > uz ? ux : uz;

    b->distance = (float)um;
    b->direction_x = (dx > 0) ? 1U : 0U;
    b->direction_z = (dz > 0) ? 1U : 0U;
    b->bl_axis = (ux >= uz) ? AXIS_X : AXIS_Z;
}

/* Блок → MotionCommand_t → dds_motion_start */
static void planner_start_block(PlannerBlock_t *b) {
    MotionCommand_t cmd;
    uint8_t master;

    memset(&cmd, 0, sizeof(cmd));

    if (b->flags & PLANNER_FLAG_BACKLASH) {
        master = b->bl_axis;
        cmd.flags = MOTION_FLAG_BACKLASH;
        cmd.bl_axis = b->bl_axis;
        cmd.bl_steps = b->bl_steps;
        cmd.target_x = dds_get_position(AXIS_X);
        cmd.target_z = dds_get_position(AXIS_Z);
        cmd.nominal_mm_min = cap_speed_mm_min(master,
            config_backlash_comp_speed_mm_min(master, b->speed));
        cmd.entry_mm_min = cap_speed_mm_min(master, b->entry_speed);
        cmd.exit_mm_min = cap_speed_mm_min(master, b->exit_speed);
        dds_motion_start(&cmd);
        block_exec = 1;
        return;
    }

    int32_t tx = b->target_steps_x;
    int32_t tz = b->target_steps_z;

    {
        int32_t dx = tx - dds_get_position(AXIS_X);
        int32_t dz = tz - dds_get_position(AXIS_Z);
        uint32_t ux = (uint32_t)((dx < 0) ? -dx : dx);
        uint32_t uz = (uint32_t)((dz < 0) ? -dz : dz);
        master = (ux >= uz) ? AXIS_X : AXIS_Z;
    }

    cmd.target_x = tx;
    cmd.target_z = tz;
    cmd.nominal_mm_min = cap_speed_mm_min(master, b->speed);
    cmd.entry_mm_min = cap_speed_mm_min(master, b->entry_speed);
    cmd.exit_mm_min = cap_speed_mm_min(master, b->exit_speed);
    cmd.flags = 0;
    dds_motion_start(&cmd);
    block_exec = 1;
}

/* После сегмента: sync люфта для backlash-блока */
static void planner_finish_block(PlannerBlock_t *b) {
    if (b->flags & PLANNER_FLAG_BACKLASH) {
        uint8_t ref = (b->bl_axis == AXIS_X) ? BACKLASH_REF_DIR_X : BACKLASH_REF_DIR_Z;
        backlash_sync_axis(b->bl_axis, ref);
    }
}

static uint8_t planner_queue_count(void) {
    if (block_head >= block_tail) {
        return (uint8_t)(block_head - block_tail);
    }
    return (uint8_t)(BLOCK_BUFFER_SIZE - block_tail + block_head);
}

static void planner_log_added_block(const PlannerBlock_t *b) {
    uint8_t bl = (b->flags & PLANNER_FLAG_BACKLASH) ? 1U : 0U;
    DBG_PLN_ADD(bl, b->bl_axis, (uint32_t)b->distance, b->target_x, b->target_z,
                b->speed, b->entry_speed, b->exit_speed, planner_queue_count());
}

static void planner_block_set_target(PlannerBlock_t *b, int32_t tx, int32_t tz) {
    b->target_steps_x = limits_clamp_steps(AXIS_X, tx);
    b->target_steps_z = limits_clamp_steps(AXIS_Z, tz);
    b->target_x = (float)b->target_steps_x / STEPS_PER_MM_X;
    b->target_z = (float)b->target_steps_z / STEPS_PER_MM_Z;
}

/* Добавить блок в head; -1 переполнение, -2 нулевая длина */
static int planner_push_block(int32_t tx, int32_t tz, float speed, uint8_t flags,
                              uint8_t bl_axis, uint16_t bl_steps) {
    uint8_t next = (uint8_t)((block_head + 1U) % BLOCK_BUFFER_SIZE);
    PlannerBlock_t *b;

    if (next == block_tail) return -1;

    b = &block_buffer[block_head];
    b->active = 1;
    planner_block_set_target(b, tx, tz);
    b->speed = speed;
    b->entry_speed = 0.0f;
    b->exit_speed = 0.0f;
    b->distance = 0.0f;
    b->direction_x = 0;
    b->direction_z = 0;
    b->flags = flags;
    b->bl_axis = bl_axis;
    b->bl_steps = bl_steps;

    block_head = next;
    planner_calculate_junctions();

    b = &block_buffer[prev_index(block_head)];
    if (!(flags & PLANNER_FLAG_BACKLASH) && b->distance < 1.0f) {
        b->active = 0;
        block_head = prev_index(block_head);
        planner_calculate_junctions();
        return -2;
    }

    planner_log_added_block(b);
    return 0;
}

int planner_add_move_steps(int32_t tx, int32_t tz, float speed) {
    int rc;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        rc = planner_push_block(tx, tz, speed, 0, 0, 0);
    }
    if (rc == -1) {
        DBG_VERBOSE("PLN", "ADD", "full");
    } else if (rc == -2) {
        DBG_VERBOSE("PLN", "ADD", "zero");
    }
    return rc;
}

int planner_add_move(float x, float z, float speed) {
    int32_t tx = planner_mm_to_steps(AXIS_X, x);
    int32_t tz = planner_mm_to_steps(AXIS_Z, z);
    return planner_add_move_steps(tx, tz, speed);
}

int planner_add_backlash_takeup(uint8_t axis, float speed_mm_min) {
    int32_t steps = backlash_get_steps(axis);
    int rc;

    if (steps <= 0) return 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        int32_t tx = dds_get_position(AXIS_X);
        int32_t tz = dds_get_position(AXIS_Z);
        rc = planner_push_block(tx, tz, speed_mm_min, PLANNER_FLAG_BACKLASH, axis, (uint16_t)steps);
    }
    if (rc == -1) {
        DBG_VERBOSE("PLN", "BL", "full");
    }
    return rc;
}

static float planner_startup_speed(uint8_t axis) {
    ui_pot_read();
    return config_backlash_comp_speed_mm_min(axis, ui_pot_get_jog_mm_min());
}

void planner_startup_backlash_queue(void) {
    startup_pending = 0;

    if (!backlash_enabled() || !config_backlash_get_auto_on()) {
        DBG_INFO("MOT", "BL", "auto skip");
        return;
    }

    ui_pot_read();

    if (backlash_get_steps(AXIS_X) > 0) {
        planner_add_backlash_takeup(AXIS_X, planner_startup_speed(AXIS_X));
        startup_pending = 1;
    }
    if (backlash_get_steps(AXIS_Z) > 0) {
        planner_add_backlash_takeup(AXIS_Z, planner_startup_speed(AXIS_Z));
        startup_pending = 1;
    }

    if (startup_pending) {
        DBG_INFO("MOT", "BL", "auto queued");
    } else {
        DBG_INFO("MOT", "BL", "auto zero");
    }
}

uint8_t planner_startup_busy(void) {
    if (!startup_pending) return 0;
    if (planner_is_busy()) return 1;
    startup_pending = 0;
    DBG_INFO("MOT", "BL", "auto done");
    return 0;
}

#define PLN_MAX_JOG_STEPS 16384U  /* запас cruise: ~0.5 с при ~30 кГц (было 4096 → рывки) */

static int32_t pln_clamp_jog_target(int32_t pos, int32_t tgt) {
    int32_t d = tgt - pos;

    if (d > (int32_t)PLN_MAX_JOG_STEPS) return pos + (int32_t)PLN_MAX_JOG_STEPS;
    if (d < -(int32_t)PLN_MAX_JOG_STEPS) return pos - (int32_t)PLN_MAX_JOG_STEPS;
    return tgt;
}

uint8_t planner_exec_jog(int32_t tx, int32_t tz, float speed_mm_min, const char *kind, uint8_t cruise,
                         uint8_t lim_hit, uint8_t lim_cmp, int32_t lim_cmp_stp) {
    TRACE_ENTER(TR_PLANNER_EXEC_JOG);
    /* retarget если jog активен; иначе stop + start; cruise = MOTION_FLAG_JOG_CRUISE */
    static MotionCommand_t cmd;
    float entry = 0.0f;
    uint8_t extended;
    uint8_t master = AXIS_Z;
    int32_t cx = dds_get_position(AXIS_X);
    int32_t cz = dds_get_position(AXIS_Z);
    int32_t dx;
    int32_t dz;
    uint32_t ux;
    uint32_t uz;

    tx = pln_clamp_jog_target(cx, tx);
    tz = pln_clamp_jog_target(cz, tz);
    dx = tx - cx;
    dz = tz - cz;
    ux = (uint32_t)((dx < 0) ? -dx : dx);
    uz = (uint32_t)((dz < 0) ? -dz : dz);

    if (ux == 0U && uz == 0U) {
        /* Джойстик у лимита: цель не сдвигается — retarget только скорости (pot) */
        if (cruise && dds_motion_jog_cruise_active()) {
            memset(&cmd, 0, sizeof(cmd));
            cmd.target_x = tx;
            cmd.target_z = tz;
            cmd.nominal_mm_min = speed_mm_min;
            cmd.flags = MOTION_FLAG_JOG | MOTION_FLAG_JOG_CRUISE;
            planner_jog_kind_flags(&cmd.flags, kind);
            if (dds_motion_jog_retarget(&cmd)) {
                return 1U;
            }
        }
        return 0U;
    }

    master = (ux >= uz) ? AXIS_X : AXIS_Z;
    speed_mm_min = cap_speed_mm_min(master, speed_mm_min);
    if (!kind) kind = "JOG";

    memset(&cmd, 0, sizeof(cmd));
    cmd.target_x = tx;
    cmd.target_z = tz;
    cmd.nominal_mm_min = speed_mm_min;
    cmd.exit_mm_min = 0.0f;
    cmd.flags = MOTION_FLAG_JOG;
    if (cruise) cmd.flags |= MOTION_FLAG_JOG_CRUISE;
    planner_jog_kind_flags(&cmd.flags, kind);

    extended = dds_motion_jog_retarget(&cmd);
    if (extended) {
        DBG_JOG_MOVE_LIM(kind, tx, tz, speed_mm_min, 1, lim_hit, lim_cmp, lim_cmp_stp);
        block_exec = 1;
        return 1U;
    }

    /* Живой jog-cruise: не stop+start (рывки разгона / повторный CRUISE) */
    if (cruise && dds_motion_jog_cruise_active()) {
        block_exec = 1;
        return 1U;
    }

    if (dds_motion_busy()) {
        {
            float ex = dds_motion_get_speed_mm_min(AXIS_X);
            float ez = dds_motion_get_speed_mm_min(AXIS_Z);
            entry = (ex > ez) ? ex : ez;
        }
        {
            uint8_t sreg = SREG;

            cli();
            dds_motion_stop();
            block_exec = 0;
            cmd.entry_mm_min = cap_speed_mm_min(master, entry);
            dds_motion_start(&cmd);
            SREG = sreg;
        }
        block_exec = 1;
        DBG_JOG_MOVE_LIM(kind, tx, tz, speed_mm_min, 0, lim_hit, lim_cmp, lim_cmp_stp);
        return 1U;
    }

    cmd.entry_mm_min = cap_speed_mm_min(master, entry);
    dds_motion_start(&cmd);
    block_exec = 1;
    DBG_JOG_MOVE_LIM(kind, tx, tz, speed_mm_min, 0, lim_hit, lim_cmp, lim_cmp_stp);
    return 1U;
}

void planner_jog_halt(void) {
    TRACE_ENTER(TR_PLANNER_JOG_HALT);
    uint8_t sreg = SREG;
    int32_t tx;
    int32_t tz;

    cli();
    dds_motion_stop();
    backlash_abort_pending();
    tx = dds_get_position(AXIS_X);
    tz = dds_get_position(AXIS_Z);
    dds_set_target(AXIS_X, tx);
    dds_set_target(AXIS_Z, tz);
    SREG = sreg;
    block_exec = 0;
}

void planner_jog_stop(void) {
    TRACE_ENTER(TR_PLANNER_JOG_STOP);
    uint8_t was_busy = dds_motion_busy();
    int32_t tx;
    int32_t tz;

    if (was_busy || backlash_pending(AXIS_X) > 0 || backlash_pending(AXIS_Z) > 0) {
        dds_motion_jog_release();
        block_exec = 0;
        tx = dds_get_position(AXIS_X);
        tz = dds_get_position(AXIS_Z);
        if (was_busy) {
            DBG_JOG_STOP(tx, tz);
        }
        return;
    }

    dds_set_target(AXIS_X, dds_get_position(AXIS_X));
    dds_set_target(AXIS_Z, dds_get_position(AXIS_Z));
}

void planner_exec_axis(uint8_t axis, int32_t target_steps, float speed_mm_min, uint8_t jog) {
    int32_t tx = dds_get_position(AXIS_X);
    int32_t tz = dds_get_position(AXIS_Z);

    if (axis > AXIS_Z) return;

    if (axis == AXIS_X) {
        tx = target_steps;
    } else {
        tz = target_steps;
    }

    if (jog) {
        planner_exec_jog(tx, tz, speed_mm_min, "JOG", 0, 0U, 0U, 0L);
        return;
    }

    if (dds_motion_busy()) {
        return;
    }

    {
        MotionCommand_t cmd;
        uint8_t master = axis;

        memset(&cmd, 0, sizeof(cmd));
        cmd.target_x = tx;
        cmd.target_z = tz;
        cmd.nominal_mm_min = cap_speed_mm_min(master, speed_mm_min);
        cmd.entry_mm_min = 0.0f;
        cmd.exit_mm_min = 0.0f;
        cmd.flags = 0;
        dds_motion_start(&cmd);
        block_exec = 1;
        DBG_JOG_MOVE("LIM", tx, tz, speed_mm_min, 0);
    }
}

void planner_calculate_junctions(void) {  /* GRBL-style: entry_speed от exit следующего */
    if (block_tail == block_head) return;

    uint8_t idx = block_tail;
    int32_t cx = dds_get_position(AXIS_X);
    int32_t cz = dds_get_position(AXIS_Z);

    while (idx != block_head) {
        PlannerBlock_t *b = &block_buffer[idx];
        if (!b->active) break;

        planner_fill_geometry_segment(b, cx, cz);

        if (b->flags & PLANNER_FLAG_BACKLASH) {
            /* позиция не сдвигается */
        } else {
            cx = b->target_steps_x;
            cz = b->target_steps_z;
        }
        idx = next_index(idx);
    }

    idx = prev_index(block_head);
    float next_entry = 0.0f;

    while (1) {
        PlannerBlock_t *b = &block_buffer[idx];
        if (!b->active) break;

        b->exit_speed = next_entry;

        if (b->flags & PLANNER_FLAG_BACKLASH) {
            b->entry_speed = 0.0f;
            b->exit_speed = 0.0f;
            next_entry = 0.0f;
        } else {
            uint8_t master = b->bl_axis;
            float max_entry = speed_from_dist(b->exit_speed, b->distance, master);
            if (max_entry > b->speed) max_entry = b->speed;
            b->entry_speed = max_entry;
            next_entry = max_entry;
        }

        if (idx == block_tail) break;
        idx = prev_index(idx);
    }
}

void planner_process(void) {  /* main: лог DDS, завершение блока, старт следующего */
    dds_motion_log_poll();
    backlash_log_poll();

    if (block_exec) {
        if (!dds_motion_busy()) {
            if (block_tail != block_head) {
                PlannerBlock_t *done = &block_buffer[block_tail];
                planner_finish_block(done);
                done->active = 0;
                block_tail = (uint8_t)((block_tail + 1U) % BLOCK_BUFFER_SIZE);
            }
            block_exec = 0;
        } else {
            return;
        }
    }

    if (block_tail == block_head) return;

    PlannerBlock_t *b = &block_buffer[block_tail];
    if (!b->active) {
        block_tail = (uint8_t)((block_tail + 1U) % BLOCK_BUFFER_SIZE);
        return;
    }

    planner_start_block(b);
}

PlannerBlock_t *planner_get_next(void) {
    if (block_tail == block_head) return NULL;
    PlannerBlock_t *b = &block_buffer[block_tail];
    if (!b->active) return NULL;
    block_tail = (uint8_t)((block_tail + 1U) % BLOCK_BUFFER_SIZE);
    return b;
}

void planner_stop_all(void) {
    block_head = 0;
    block_tail = 0;
    block_exec = 0;
    startup_pending = 0;
    dds_motion_stop();
}

uint8_t planner_is_empty(void) {
    return block_tail == block_head;
}

uint8_t planner_is_busy(void) {
    return block_exec || dds_motion_busy() || !planner_is_empty();
}

void planner_get_queue_target(float *x, float *z) {
    int32_t tx;
    int32_t tz;

    if (!x || !z) return;
    planner_get_queue_target_steps(&tx, &tz);
    *x = (float)tx / STEPS_PER_MM_X;
    *z = (float)tz / STEPS_PER_MM_Z;
}

void planner_get_queue_target_steps(int32_t *tx, int32_t *tz) {
    if (!tx || !tz) return;

    if (block_tail == block_head) {
        *tx = dds_get_position(AXIS_X);
        *tz = dds_get_position(AXIS_Z);
        return;
    }

    PlannerBlock_t *b = &block_buffer[prev_index(block_head)];
    *tx = b->target_steps_x;
    *tz = b->target_steps_z;
}

void planner_set_acceleration(float accel) {
    (void)accel;
}

void planner_set_max_speed(float speed) {
    (void)speed;
}
