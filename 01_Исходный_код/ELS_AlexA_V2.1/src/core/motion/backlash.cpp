#include "backlash.h"
#include "../../config/config.h"

static int32_t backlash_comp_x = 0;
static int32_t backlash_comp_z = 0;
static uint8_t last_dir_x = 0;
static uint8_t last_dir_z = 0;
static uint8_t backlash_initialized = 0;

void backlash_init(void) {
#ifdef ENABLE_BACKLASH
    backlash_comp_x = BACKLASH_X;
    backlash_comp_z = BACKLASH_Z;
    backlash_initialized = 1;
#else
    backlash_comp_x = 0;
    backlash_comp_z = 0;
    backlash_initialized = 0;
#endif
}

int32_t backlash_compensate(uint8_t axis, int32_t steps, uint8_t direction) {
#ifdef ENABLE_BACKLASH
    if (!backlash_initialized) return steps;
    if (axis == AXIS_X) {
        if (direction != last_dir_x) {
            steps += (direction ? backlash_comp_x : -backlash_comp_x);
            last_dir_x = direction;
        }
    } else {
        if (direction != last_dir_z) {
            steps += (direction ? backlash_comp_z : -backlash_comp_z);
            last_dir_z = direction;
        }
    }
#endif
    return steps;
}

void backlash_set_distance(uint8_t axis, int32_t distance) {
    if (axis == AXIS_X) {
        backlash_comp_x = distance;
    } else {
        backlash_comp_z = distance;
    }
}

uint8_t backlash_enabled(void) {
#ifdef ENABLE_BACKLASH
    return 1;
#else
    return 0;
#endif
}

void backlash_reset(void) {
    last_dir_x = 0;
    last_dir_z = 0;
}

int32_t backlash_get_comp_x(void) {
    return backlash_comp_x;
}

int32_t backlash_get_comp_z(void) {
    return backlash_comp_z;
}
