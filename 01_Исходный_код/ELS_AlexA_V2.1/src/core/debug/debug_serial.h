#ifndef DEBUG_SERIAL_H
#define DEBUG_SERIAL_H

#include "../../config/config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void debug_serial_init(uint32_t baud);
void debug_serial_print(const char *msg);
void debug_serial_println(const char *msg);
void debug_serial_print_val(const char *label, uint32_t val);
void debug_serial_enable(uint8_t enable);

void debug_log_event(uint8_t level, const char *module, const char *component, const char *msg);
void debug_log_event_val(uint8_t level, const char *module, const char *component, const char *msg, uint32_t val);
void debug_log_event_val_i32(uint8_t level, const char *module, const char *component, const char *msg, int32_t val);
void debug_log_jog(int32_t steps, uint8_t axis, int32_t coord_steps);
void debug_log_jog_move(const char *kind, int32_t tx, int32_t tz, float spd, uint8_t extend);
void debug_log_planner_add(uint8_t backlash, uint8_t axis, uint32_t dst_steps,
                           float x_mm, float z_mm, float speed_mm_min,
                           float entry_mm_min, float exit_mm_min, uint8_t queue_len);
void debug_log_backlash(const char *evt, uint8_t axis, uint8_t dir, int32_t val);
void debug_log_planner_dir(uint8_t axis, int8_t dir, int32_t pos, int32_t tgt);
void debug_log_planner_cruise(uint8_t axis, uint32_t step_ev, uint32_t rate_sps,
                              uint32_t nominal_sps, int32_t pos);
void debug_log_planner_decel(uint8_t axis, uint32_t step_ev, uint32_t rate_sps,
                             uint32_t dec_after, uint32_t step_cnt, int32_t pos);

#if DEBUG_ENABLED
#define DBG_INFO(mod, comp, msg) debug_log_event(DEBUG_LEVEL_INFO, mod, comp, msg)
#define DBG_INFO_VAL(mod, comp, msg, v) debug_log_event_val(DEBUG_LEVEL_INFO, mod, comp, msg, v)
#define DBG_INFO_VAL_I32(mod, comp, msg, v) debug_log_event_val_i32(DEBUG_LEVEL_INFO, mod, comp, msg, v)
#define DBG_VERBOSE(mod, comp, msg) debug_log_event(DEBUG_LEVEL_VERBOSE, mod, comp, msg)
#define DBG_VERBOSE_VAL(mod, comp, msg, v) debug_log_event_val(DEBUG_LEVEL_VERBOSE, mod, comp, msg, v)
#define DBG_VERBOSE_VAL_I32(mod, comp, msg, v) debug_log_event_val_i32(DEBUG_LEVEL_VERBOSE, mod, comp, msg, v)
#define DBG_JOG(steps, axis, coord) debug_log_jog(steps, axis, coord)
#define DBG_JOG_MOVE(kind, tx, tz, spd, ext) debug_log_jog_move(kind, tx, tz, spd, ext)
#define DBG_PLN_ADD(bl, ax, dst, x, z, spd, ein, eout, q) \
    debug_log_planner_add(bl, ax, dst, x, z, spd, ein, eout, q)
#define DBG_BL(evt, axis, dir, val) debug_log_backlash(evt, axis, dir, val)
#define DBG_PLN_DIR(axis, dir, pos, tgt) debug_log_planner_dir(axis, dir, pos, tgt)
#define DBG_PLN_CRUISE(axis, stp, rate, nom, pos) \
    debug_log_planner_cruise(axis, stp, rate, nom, pos)
#define DBG_PLN_DECEL(axis, stp, rate, dec, cnt, pos) \
    debug_log_planner_decel(axis, stp, rate, dec, cnt, pos)
#else
#define DBG_INFO(mod, comp, msg) ((void)0)
#define DBG_INFO_VAL(mod, comp, msg, v) ((void)0)
#define DBG_INFO_VAL_I32(mod, comp, msg, v) ((void)0)
#define DBG_JOG(steps, axis, coord) ((void)0)
#define DBG_JOG_MOVE(kind, tx, tz, spd, ext) ((void)0)
#define DBG_PLN_ADD(bl, ax, dst, x, z, spd, ein, eout, q) ((void)0)
#define DBG_VERBOSE(mod, comp, msg) ((void)0)
#define DBG_VERBOSE_VAL(mod, comp, msg, v) ((void)0)
#define DBG_VERBOSE_VAL_I32(mod, comp, msg, v) ((void)0)
#define DBG_BL(evt, axis, dir, val) ((void)0)
#define DBG_PLN_DIR(axis, dir, pos, tgt) ((void)0)
#define DBG_PLN_CRUISE(axis, stp, rate, nom, pos) ((void)0)
#define DBG_PLN_DECEL(axis, stp, rate, dec, cnt, pos) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif
