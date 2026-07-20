#ifndef CONFIG_H
#define CONFIG_H

/* Главный заголовок конфигурации: платформа, тайминги, отладка, макросы движения.
 * Подключает config_storage (через load), machine, backlash, display. */

#include <stdint.h>

#define FIRMWARE_NAME "ELS AlexA V2.1"   /* имя прошивки для LCD / Serial */
#define FIRMWARE_STAGE "Stage 2.2f"      /* этап разработки (строка статуса) */
#define FIRMWARE_LCD_VER "V2.1 Stage 2.2f" /* короткая версия для LCD MODE_OFF */

#define PLATFORM_ARDUINO_MEGA 1          /* целевая плата: Arduino Mega 2560 */
#define CPU_FREQ 16000000UL              /* частота CPU, Гц (16 МГц ATmega2560) */

#define SERIAL_BAUD 115200               /* скорость отладочного Serial, бод */

#define LCD_COLS 20                      /* ширина LCD HD44780 (символов) */
#define LCD_ROWS 4                       /* высота LCD (строк) */

#define LCD_UPDATE_MS 80                  /* период обновления дисплея, мс */
#define POT_READ_MS 100                  /* период опроса потенциометра подачи, мс */

#define DEBUG_ENABLED 1                  /* 1 — включить DBG_* вывод в Serial */

#define DEBUG_LEVEL_ERROR   0            /* только ошибки */
#define DEBUG_LEVEL_WARNING 1            /* + предупреждения */
#define DEBUG_LEVEL_INFO    2            /* + информационные */
#define DEBUG_LEVEL_VERBOSE 3            /* максимум детализации */

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DEBUG_LEVEL_VERBOSE     /* VERBOSE рвёт плавность РГИ (Serial) */
#endif

/* Трассировка краша: TRACE_ENTER(id) в цепочке MPG/jog (debug_trace.h) */
#ifndef TRACE_CRASH_ENABLED
#define TRACE_CRASH_ENABLED  0           /* 1 — кольцевой буфер TRACE_ENTER */
#endif
/* 1 — печать >NN в Serial при каждом TRACE_ENTER (main); 0 — только LST/ISR при ребуте */
#ifndef TRACE_CRASH_SERIAL
#define TRACE_CRASH_SERIAL   0
#endif

#define BTN_DEBOUNCE_MS 50               /* антидребезг кнопок, мс */

#define STEP_ISR_PERIOD_US 40            /* период ISR шагов, мкс (~25 кГц; 33 не хватало на cruise) */
#define STEP_ISR_FREQ_HZ (1000000UL / STEP_ISR_PERIOD_US) /* частота ISR, Гц */

#include "config_defs.h"
#include "config_mpg.h"
#include "config_machine.h"
#include "config_backlash.h"
#include "config_lcd.h"
#include "config_display.h"

/* Рантайм-значения из EEPROM (config_machine) */
#define STEPS_PER_MM_X config_get_steps_per_mm(AXIS_X)   /* шагов/мм оси X */
#define STEPS_PER_MM_Z config_get_steps_per_mm(AXIS_Z)   /* шагов/мм оси Z */
#define SPINDLE_ENCODER_PPR config_get_spindle_ppr()     /* PPR энкодера шпинделя */

#endif
