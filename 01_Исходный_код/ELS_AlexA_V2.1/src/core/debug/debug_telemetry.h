#ifndef DEBUG_TELEMETRY_H
#define DEBUG_TELEMETRY_H






#include "../../config/config.h"
#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


/* --- Уровни отладки --- */





/* --- Структура телеметрии --- */
typedef struct {
uint32_t timestamp;
uint8_t level;
const char* module;
const char* component;
const char* message;
uint32_t value;
} TelemetryEntry_t;

/* --- Основные функции --- */
void debug_telemetry_init(void);
void debug_telemetry_log(uint8_t level, const char* module,
const char* component, const char* msg, uint32_t val);
void debug_telemetry_send(void);
void debug_telemetry_clear(void);

/* --- Макросы для удобства --- */

#if DEBUG_ENABLED
#define DEBUG_LOG(module, component, msg, val) \
    debug_telemetry_log(DEBUG_LEVEL_INFO, module, component, msg, val)

#define DEBUG_WARN(module, component, msg, val) \
    debug_telemetry_log(DEBUG_LEVEL_WARNING, module, component, msg, val)

#define DEBUG_ERROR(module, component, msg, val) \
    debug_telemetry_log(DEBUG_LEVEL_ERROR, module, component, msg, val)
#else
#define DEBUG_LOG(module, component, msg, val) ((void)0)
#define DEBUG_WARN(module, component, msg, val) ((void)0)
#define DEBUG_ERROR(module, component, msg, val) ((void)0)
#endif


#ifdef __cplusplus
}
#endif
#endif
