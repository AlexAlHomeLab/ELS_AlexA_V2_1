#include "planner.h"
#include "stepper_gen.h"
#include "limits.h"
#include "../../config/config.h"
#include <math.h>
#include <stddef.h>

static PlannerBlock_t block_buffer[BLOCK_BUFFER_SIZE];
static uint8_t block_head = 0;
static uint8_t block_tail = 0;
static uint8_t block_exec = 0;

static uint32_t mm_min_to_sps(float mm_min, float steps_per_mm) {
    if (mm_min < 1.0f) mm_min = 100.0f;
    uint32_t sps = (uint32_t)((mm_min * steps_per_mm) / 60.0f);
    if (sps < JOG_SPEED_MIN_SPS) sps = JOG_SPEED_MIN_SPS;
    if (sps > JOG_SPEED_MAX_SPS) sps = JOG_SPEED_MAX_SPS;
    return sps;
}

int planner_add_move(float x, float z, float speed) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        uint8_t next = (block_head + 1) % BLOCK_BUFFER_SIZE;
        if (next == block_tail) return -1;

        PlannerBlock_t *b = &block_buffer[block_head];
        b->active = 1;
        b->target_x = x;
        b->target_z = z;
        b->speed = speed;
        b->entry_speed = 0;
        b->exit_speed = 0;
        b->distance = 0;
        b->direction_x = 0;
        b->direction_z = 0;
        planner_calculate_junctions();
        block_head = next;
    }
    return 0;
}

void planner_calculate_junctions(void) {
}

static void planner_start_block(PlannerBlock_t *b) {
    int32_t cx = dds_get_position(AXIS_X);
    int32_t cz = dds_get_position(AXIS_Z);
    int32_t tx = (int32_t)(b->target_x * STEPS_PER_MM_X);
    int32_t tz = (int32_t)(b->target_z * STEPS_PER_MM_Z);
    tx = limits_clamp_steps(AXIS_X, tx);
    tz = limits_clamp_steps(AXIS_Z, tz);
    int32_t dx = tx - cx;
    int32_t dz = tz - cz;
    uint32_t ux = (uint32_t)((dx < 0) ? -dx : dx);
    uint32_t uz = (uint32_t)((dz < 0) ? -dz : dz);
    uint32_t um = ux > uz ? ux : uz;

    if (um == 0) {
        return;
    }

    uint32_t base_sps = mm_min_to_sps(b->speed, STEPS_PER_MM_Z);

    dds_set_target(AXIS_X, tx);
    dds_set_target(AXIS_Z, tz);

    if (ux > 0) {
        uint32_t sx = (uint32_t)(((uint64_t)base_sps * ux) / um);
        if (sx < 1) sx = 1;
        dds_set_speed(AXIS_X, sx);
        dds_enable(AXIS_X, 1);
    } else {
        dds_enable(AXIS_X, 0);
    }

    if (uz > 0) {
        uint32_t sz = (uint32_t)(((uint64_t)base_sps * uz) / um);
        if (sz < 1) sz = 1;
        dds_set_speed(AXIS_Z, sz);
        dds_enable(AXIS_Z, 1);
    } else {
        dds_enable(AXIS_Z, 0);
    }

    block_exec = 1;
}

void planner_process(void) {
    if (block_exec) {
        if (dds_at_target(AXIS_X) && dds_at_target(AXIS_Z)) {
            block_exec = 0;
            dds_enable(AXIS_X, 0);
            dds_enable(AXIS_Z, 0);
        } else {
            return;
        }
    }

    if (block_tail == block_head) return;

    PlannerBlock_t *b = &block_buffer[block_tail];
    if (!b->active) {
        block_tail = (block_tail + 1) % BLOCK_BUFFER_SIZE;
        return;
    }

    planner_start_block(b);
    b->active = 0;
    block_tail = (block_tail + 1) % BLOCK_BUFFER_SIZE;

    if (!block_exec) {
        planner_process();
    }
}

PlannerBlock_t *planner_get_next(void) {
    if (block_tail == block_head) return NULL;
    PlannerBlock_t *b = &block_buffer[block_tail];
    if (!b->active) return NULL;
    block_tail = (block_tail + 1) % BLOCK_BUFFER_SIZE;
    return b;
}

void planner_stop_all(void) {
    block_head = 0;
    block_tail = 0;
    block_exec = 0;
}

uint8_t planner_is_empty(void) {
    return block_tail == block_head;
}

uint8_t planner_is_busy(void) {
    return block_exec || !planner_is_empty();
}

void planner_set_acceleration(float accel) {
    (void)accel;
}

void planner_set_max_speed(float speed) {
    (void)speed;
}
