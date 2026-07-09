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

#if DEBUG_ENABLED
#define DBG_INFO(mod, comp, msg) debug_log_event(DEBUG_LEVEL_INFO, mod, comp, msg)
#define DBG_INFO_VAL(mod, comp, msg, v) debug_log_event_val(DEBUG_LEVEL_INFO, mod, comp, msg, v)
#else
#define DBG_INFO(mod, comp, msg) ((void)0)
#define DBG_INFO_VAL(mod, comp, msg, v) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif
