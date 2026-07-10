#ifndef CONFIG_H
#define CONFIG_H

/* Главный заголовок конфигурации: платформа, тайминги, отладка, макросы движения.
 * Подключает config_storage (через load), machine, backlash, display. */

#include <stdint.h>

#define FIRMWARE_NAME "ELS AlexA V2.1"
#define FIRMWARE_STAGE "Stage 2.2f"

#define PLATFORM_ARDUINO_MEGA 1
#define CPU_FREQ 16000000UL       /* 16 МГц ATmega2560 */

#define SERIAL_BAUD 115200

#define LCD_COLS 20
#define LCD_ROWS 4

#define LCD_UPDATE_MS 80          /* период обновления дисплея */
#define POT_READ_MS 100           /* период опроса потенциометра */

#define DEBUG_ENABLED 1

#define DEBUG_LEVEL_ERROR   0
#define DEBUG_LEVEL_WARNING 1
#define DEBUG_LEVEL_INFO    2
#define DEBUG_LEVEL_VERBOSE 3

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_INFO
#endif

#define BTN_DEBOUNCE_MS 50

#define STEP_ISR_PERIOD_US 33     /* период ISR шагов, мкс (~30 кГц) */
#define STEP_ISR_FREQ_HZ (1000000UL / STEP_ISR_PERIOD_US)

#include "config_defs.h"
#include "config_machine.h"
#include "config_backlash.h"
#include "config_display.h"

/* Рантайм-значения из EEPROM (config_machine) */
#define STEPS_PER_MM_X config_get_steps_per_mm(AXIS_X)
#define STEPS_PER_MM_Z config_get_steps_per_mm(AXIS_Z)
#define SPINDLE_ENCODER_PPR config_get_spindle_ppr()

#endif
