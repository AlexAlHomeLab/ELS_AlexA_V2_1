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

/* --- Инверсия DIR (0/1), значение по умолчанию --- */
#ifndef AXIS_X_DIR_INVERT_DEFAULT
#define AXIS_X_DIR_INVERT_DEFAULT 0
#endif
#ifndef AXIS_Z_DIR_INVERT_DEFAULT
#define AXIS_Z_DIR_INVERT_DEFAULT 0
#endif

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

/* --- Компенсация люфта (ТЗ: выборка при смене направления, ENABLE_BACKLASH=0 откл.) --- */
#define ENABLE_BACKLASH         1
#define BACKLASH_DIR_POS        1   /* направление увеличения координаты */
#define BACKLASH_DIR_NEG        0
#define BACKLASH_REF_DIR_X      BACKLASH_DIR_NEG
#define BACKLASH_REF_DIR_Z      BACKLASH_DIR_NEG
#define BACKLASH_FIRST_SKIP     0
#define BACKLASH_FIRST_ALWAYS   1   /* первый ход после вкл. — выборка (ТЗ) */
#define BACKLASH_FIRST_REF      2
#define BACKLASH_FIRST_MOVE     BACKLASH_FIRST_ALWAYS
/* Дистанция выборки по умолчанию (импульсы); EEPROM переопределяет */
#define BACKLASH_X_STEPS_DEFAULT    100
#define BACKLASH_Z_STEPS_DEFAULT    100
#define BACKLASH_X_CENTIMM            34
#define BACKLASH_Z_CENTIMM            46
#define BACKLASH_STEPS_MAX            5000
/* Автовыборка при включении (EEPROM $40 / меню BlAu) */
#define BACKLASH_AUTO_DEFAULT         1
#define BACKLASH_AUTO_SPEED_DEFAULT   50
#define BACKLASH_AUTO_SPEED_MIN       5
#define BACKLASH_AUTO_SPEED_MAX       200

/* --- Телеметрия --- */
#define TELEMETRY_BUFFER_SIZE 64

#endif
