#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define FIRMWARE_NAME "ELS AlexA V2.1"
#define FIRMWARE_STAGE "Stage 1"

#define PLATFORM_ARDUINO_MEGA 1
#define CPU_FREQ 16000000UL

#define SERIAL_BAUD 115200

#define STEPS_PER_MM_X 200.0f
#define STEPS_PER_MM_Z 200.0f

#define SPINDLE_ENCODER_PPR 3000

#define LCD_COLS 20
#define LCD_ROWS 4

#define LCD_UPDATE_MS 80
#define POT_READ_MS 100

#define DEBUG_ENABLED 1

#define DEBUG_LEVEL_ERROR 0
#define DEBUG_LEVEL_WARNING 1
#define DEBUG_LEVEL_INFO 2
#define DEBUG_LEVEL_VERBOSE 3

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_INFO
#endif

#define BTN_DEBOUNCE_MS 50

#include "config_defs.h"

#endif
