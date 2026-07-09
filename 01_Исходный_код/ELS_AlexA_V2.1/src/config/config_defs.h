#ifndef CONFIG_DEFS_H
#define CONFIG_DEFS_H

#include <stdint.h>

/* --- Оси --- */
#define AXIS_X 0
#define AXIS_Z 1

/* --- Подрежимы --- */
#define SUB_MANUAL 0
#define SUB_EXT    1
#define SUB_INT    2

/* --- FSM --- */
#define ERR_INVALID_TRANSITION 1
#define ERR_LIMIT              2

#define CYCLE_EXT 0
#define CYCLE_INT 1

/* --- Планировщик --- */
#define BLOCK_BUFFER_SIZE 16

#if defined(__AVR__)
#include <util/atomic.h>
#else
#ifndef ATOMIC_BLOCK
#define ATOMIC_BLOCK(type) for (uint8_t _atomic_lock = 1; _atomic_lock; _atomic_lock = 0)
#define ATOMIC_RESTORESTATE
#endif
#endif

/* --- Люфт --- */
#define ENABLE_BACKLASH 0
#define BACKLASH_X      0
#define BACKLASH_Z      0

/* --- Телеметрия --- */
#define TELEMETRY_BUFFER_SIZE 64

#endif
