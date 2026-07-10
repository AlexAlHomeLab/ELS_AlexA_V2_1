#include "../../config/config.h"
#include "../../config/config_defs.h"
#include "../../config/config_machine.h"
#include "debug_serial.h"
#include <Arduino.h>

static uint8_t serial_enabled = 1;

static const char *level_tag(uint8_t level) {
    switch (level) {
        case DEBUG_LEVEL_ERROR: return "ERROR";
        case DEBUG_LEVEL_WARNING: return "WARN";
        case DEBUG_LEVEL_VERBOSE: return "VERB";
        default: return "INFO";
    }
}

void debug_serial_init(uint32_t baud) {
    Serial.begin(baud);
    serial_enabled = 1;
}

void debug_serial_print(const char *msg) {
    if (!serial_enabled) return;
    Serial.print(msg);
}

void debug_serial_println(const char *msg) {
    if (!serial_enabled) return;
    Serial.println(msg);
}

void debug_serial_print_val(const char *label, uint32_t val) {
    if (!serial_enabled) return;
    Serial.print(label);
    Serial.print(": ");
    Serial.println(val);
}

void debug_serial_enable(uint8_t enable) {
    serial_enabled = enable;
}

void debug_log_event(uint8_t level, const char *module, const char *component, const char *msg) {
#if DEBUG_ENABLED
    if (!serial_enabled || level > DEBUG_LEVEL) return;
    Serial.print("[");
    Serial.print(level_tag(level));
    Serial.print("] [");
    Serial.print(module);
    Serial.print("] [");
    Serial.print(component);
    Serial.print("] ");
    Serial.println(msg);
#endif
}

void debug_log_event_val(uint8_t level, const char *module, const char *component, const char *msg, uint32_t val) {
#if DEBUG_ENABLED
    if (!serial_enabled || level > DEBUG_LEVEL) return;
    Serial.print("[");
    Serial.print(level_tag(level));
    Serial.print("] [");
    Serial.print(module);
    Serial.print("] [");
    Serial.print(component);
    Serial.print("] ");
    Serial.print(msg);
    Serial.print(' ');
    Serial.println(val);
#endif
}

void debug_log_event_val_i32(uint8_t level, const char *module, const char *component, const char *msg, int32_t val) {
#if DEBUG_ENABLED
    if (!serial_enabled || level > DEBUG_LEVEL) return;
    Serial.print("[");
    Serial.print(level_tag(level));
    Serial.print("] [");
    Serial.print(module);
    Serial.print("] [");
    Serial.print(component);
    Serial.print("] ");
    Serial.print(msg);
    Serial.print(' ');
    Serial.println(val);
#endif
}

void debug_log_jog(int32_t steps, uint8_t axis, int32_t coord_steps) {
#if DEBUG_ENABLED
    if (!serial_enabled || DEBUG_LEVEL_INFO > DEBUG_LEVEL) return;
    int32_t st = (steps < 0) ? -steps : steps;
    Serial.print("[INFO] [MOT] [JOG] ");
    Serial.print(axis == AXIS_X ? 'X' : 'Z');
    Serial.print("stp ");
    Serial.print(st);
    Serial.print(' ');
    Serial.print(axis == AXIS_X ? 'X' : 'Z');
    Serial.println(coord_steps);
#endif
}

void debug_log_jog_move(const char *kind, int32_t tx, int32_t tz, float spd, uint8_t extend,
                        uint8_t lim_hit, uint8_t lim_cmp, int32_t lim_cmp_stp) {
#if DEBUG_ENABLED
    uint8_t lvl = extend ? DEBUG_LEVEL_VERBOSE : DEBUG_LEVEL_INFO;
    if (!serial_enabled || lvl > DEBUG_LEVEL) return;
    Serial.print("[");
    Serial.print(extend ? "VERB" : "INFO");
    Serial.print("] [MOT] [");
    Serial.print(kind);
    Serial.print("] X");
    Serial.print(tx);
    Serial.print(" Z");
    Serial.print(tz);
    Serial.print(" F");
    Serial.print((int)spd);
    if (extend) Serial.print(" ext");
    if (lim_hit) Serial.print(" lim");
    if (lim_cmp) {
        Serial.print(" lcmp ");
        Serial.print(lim_cmp_stp);
    }
    Serial.println();
#endif
}

void debug_log_jog_stop(int32_t tx, int32_t tz) {
#if DEBUG_ENABLED
    if (!serial_enabled || DEBUG_LEVEL_INFO > DEBUG_LEVEL) return;
    Serial.print("[INFO] [MOT] [JOG] stop X");
    Serial.print(tx);
    Serial.print(" Z");
    Serial.println(tz);
#endif
}

void debug_log_mpg_pulse(uint8_t axis, int32_t pos) {
#if DEBUG_ENABLED
    if (!serial_enabled || DEBUG_LEVEL_INFO > DEBUG_LEVEL) return;
    Serial.print("[INFO] [MOT] [MPG] ");
    Serial.print(axis == AXIS_X ? 'X' : 'Z');
    Serial.print(' ');
    Serial.println(pos);
#endif
}

void debug_log_limit_cmp(uint8_t axis, int32_t dist_stp) {
#if DEBUG_ENABLED
    if (!serial_enabled || DEBUG_LEVEL_INFO > DEBUG_LEVEL) return;
    Serial.print("[INFO] [MOT] [LIM] ");
    Serial.print(axis == AXIS_X ? 'X' : 'Z');
    Serial.print(" lcmp ");
    Serial.println(dist_stp);
#endif
}

void debug_log_planner_add(uint8_t backlash, uint8_t axis, uint32_t dst_steps,
                           float x_mm, float z_mm, float speed_mm_min,
                           float entry_mm_min, float exit_mm_min, uint8_t queue_len) {
#if DEBUG_ENABLED
    if (!serial_enabled || DEBUG_LEVEL_INFO > DEBUG_LEVEL) return;
    Serial.print("[INFO] [PLN] [");
    Serial.print(backlash ? "BL" : "ADD");
    Serial.print("] ");
    if (backlash) {
        Serial.print(axis == AXIS_X ? 'X' : 'Z');
        Serial.print(" stp");
        Serial.print(dst_steps);
    } else {
        Serial.print("X");
        Serial.print(x_mm, 3);
        Serial.print(" Z");
        Serial.print(z_mm, 3);
        Serial.print(" dst");
        Serial.print(dst_steps);
    }
    Serial.print(" F");
    Serial.print((int)(speed_mm_min + 0.5f));
    Serial.print(" Ein");
    Serial.print((int)(entry_mm_min + 0.5f));
    Serial.print(" Eout");
    Serial.print((int)(exit_mm_min + 0.5f));
    Serial.print(" q");
    Serial.println(queue_len);
#endif
}

void debug_log_backlash(const char *evt, uint8_t axis, uint8_t dir, int32_t val) {
#if DEBUG_ENABLED
    if (!serial_enabled || DEBUG_LEVEL_INFO > DEBUG_LEVEL) return;
    Serial.print("[INFO] [MOT] [BL] ");
    Serial.print(evt);
    Serial.print(' ');
    Serial.print(axis == AXIS_X ? 'X' : 'Z');
    Serial.print(" dir");
    Serial.print(dir ? '+' : '-');
    if (val != 0) {
        Serial.print(" stp");
        Serial.print(val);
    }
    Serial.println();
#endif
}

static void debug_pln_print_axis(uint8_t axis) {
    Serial.print(axis == AXIS_X ? 'X' : 'Z');
}

static int debug_pln_rate_to_f(uint8_t axis, uint32_t rate_sps) {
    float spm = config_get_steps_per_mm(axis);
    if (spm < 1.0f) return 0;
    return (int)((float)rate_sps * 60.0f / spm + 0.5f);
}

void debug_log_planner_dir(uint8_t axis, int8_t dir, int32_t pos, int32_t tgt) {
#if DEBUG_ENABLED
    if (!serial_enabled || DEBUG_LEVEL_INFO > DEBUG_LEVEL) return;
    Serial.print("[INFO] [PLN] [DIR] ");
    debug_pln_print_axis(axis);
    Serial.print(' ');
    Serial.print(dir > 0 ? '+' : '-');
    Serial.print(" pos");
    Serial.print(pos);
    Serial.print(" tgt");
    Serial.println(tgt);
#endif
}

void debug_log_planner_cruise(uint8_t axis, uint32_t step_ev, uint32_t rate_sps,
                              uint32_t nominal_sps, int32_t pos) {
#if DEBUG_ENABLED
    if (!serial_enabled || DEBUG_LEVEL_INFO > DEBUG_LEVEL) return;
    Serial.print("[INFO] [PLN] [CRUISE] ");
    debug_pln_print_axis(axis);
    Serial.print(" stp");
    Serial.print(step_ev);
    Serial.print(" F");
    Serial.print(debug_pln_rate_to_f(axis, rate_sps));
    Serial.print(" Fnom");
    Serial.print(debug_pln_rate_to_f(axis, nominal_sps));
    Serial.print(" pos");
    Serial.println(pos);
#endif
}

void debug_log_planner_decel(uint8_t axis, uint32_t step_ev, uint32_t rate_sps,
                             uint32_t dec_after, uint32_t step_cnt, int32_t pos) {
#if DEBUG_ENABLED
    if (!serial_enabled || DEBUG_LEVEL_INFO > DEBUG_LEVEL) return;
    uint32_t remain = (step_cnt > step_ev) ? (step_cnt - step_ev) : 0U;
    Serial.print("[INFO] [PLN] [DECEL] ");
    debug_pln_print_axis(axis);
    Serial.print(" stp");
    Serial.print(step_ev);
    Serial.print(" F");
    Serial.print(debug_pln_rate_to_f(axis, rate_sps));
    Serial.print(" dec");
    Serial.print(dec_after);
    Serial.print(" rem");
    Serial.print(remain);
    Serial.print(" pos");
    Serial.println(pos);
#endif
}
