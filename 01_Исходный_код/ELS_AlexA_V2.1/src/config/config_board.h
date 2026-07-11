#ifndef CONFIG_BOARD_H
#define CONFIG_BOARD_H

/* Разводка платы ELS v.7e2 (Arduino Mega 2560).
 * Источник: Digital_Feed_7e2_aktual/pins.h, menu.cpp MODE_SWITCH_MAP.
 *
 * 8-поз. селектор (D30..D37): маски пинов не меняются, логический режим — remap.
 * Физ. слева→направо на панели должно быть: Async, Sync, Thread, Chamfer,
 * Cone, Sphere, Divider, Grind (стандарт прошивки). Сейчас на D30..D37
 * идёт наоборот — таблица BOARD_MODE_PIN_REMAP исправляет это.
 *
 * Лимиты: кнопки LimL=D28, LimRe=D22 (как menu.cpp 7e2); LED без изменений.
 * Бипер D16: активный LOW (покой HIGH), как Beeper_On/Off в 7e2. */

#include <stdint.h>

/* scan_mode_pin() возвращает индекс 1..8 по MODE_PIN_1..8 (D30..D37).
 * Таблица переводит его в номер режима 1..8 для FSM/LCD. */
#define BOARD_MODE_PIN_REMAP \
    { 7, 8, 5, 6, 3, 4, 1, 2 }

#ifdef __cplusplus
extern "C" {
#endif

uint8_t board_remap_mode_pin(uint8_t scan_mode);

#ifdef __cplusplus
}
#endif

#endif
