#include "planner.h"
#include "../../config/config.h"
#include <math.h>
#include <stddef.h>

static PlannerBlock_t block_buffer[BLOCK_BUFFER_SIZE];
static uint8_t block_head = 0;
static uint8_t block_tail = 0;

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
        b->distance = sqrtf(x * x + z * z);
        b->direction_x = (x >= 0) ? 1 : 0;
        b->direction_z = (z >= 0) ? 1 : 0;
        planner_calculate_junctions();
        block_head = next;
    }
    return 0;
}

void planner_calculate_junctions(void) {
    for (uint8_t i = block_tail; i != block_head; i = (i + 1) % BLOCK_BUFFER_SIZE) {
        (void)i;
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
}

void planner_process(void) {
}

uint8_t planner_is_empty(void) {
    return block_tail == block_head;
}

void planner_set_acceleration(float accel) {
    (void)accel;
}

void planner_set_max_speed(float speed) {
    (void)speed;
}
