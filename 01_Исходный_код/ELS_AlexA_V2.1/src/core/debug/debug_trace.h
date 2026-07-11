#ifndef DEBUG_TRACE_H
#define DEBUG_TRACE_H

#include "../../config/config.h"
#include <stdint.h>

#if TRACE_CRASH_ENABLED

/* Таблица ID — только «глубокие» точки (без main loop) */
enum {
    TR_MPG_COMMIT        = 31,
    TR_PLANNER_EXEC_JOG  = 40,
    TR_PLANNER_JOG_STOP  = 41,
    TR_PLANNER_JOG_HALT  = 42,
    TR_DDS_MOTION_START  = 50,
    TR_DDS_JOG_RETARGET  = 51,
    TR_DDS_JOG_RELEASE   = 52,
    TR_DDS_MOTION_STOP   = 53,

    /* ISR stepper_gen — только trace_isr_last_id, без Serial */
    TR_ISR_ENTER         = 60,
    TR_ISR_AFTER_X       = 61,
    TR_ISR_AFTER_Z       = 62,
    TR_ISR_BL_DRAIN      = 63,
    TR_ISR_PROF_DONE     = 64,
    TR_ISR_AXIS_RUN      = 65,
    TR_ISR_AXIS_PULSE    = 66,
    TR_ISR_MPROF_STEP    = 67,
    TR_ISR_MPROF_RATE    = 68,
    TR_ISR_MPROF_LOG     = 69,
};

extern volatile uint8_t trace_last_id;
extern volatile uint8_t trace_isr_last_id;

void trace_crash_init(void);
void trace_crash_mark(uint8_t id);
void trace_crash_mark_isr(uint8_t id);

#define TRACE_ENTER(id)     trace_crash_mark((uint8_t)(id))
#define TRACE_ENTER_ISR(id) trace_crash_mark_isr((uint8_t)(id))

#else

#define TRACE_ENTER(id)     ((void)0)
#define TRACE_ENTER_ISR(id) ((void)0)

static inline void trace_crash_init(void) {}

#endif

#endif
