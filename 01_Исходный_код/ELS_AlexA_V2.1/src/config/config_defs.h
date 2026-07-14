#ifndef CONFIG_DEFS_H
#define CONFIG_DEFS_H

/* Общие константы проекта: оси, FSM, планировщик, люфт, координаты. */

#include <stdint.h>

/* --- Оси --- */
#define AXIS_X 0                         /* индекс оси X (поперечная) */
#define AXIS_Z 1                         /* индекс оси Z (продольная) */

/* --- Подрежимы (Manual / External / Internal цикл) --- */
#define SUB_MANUAL 0                     /* ручной цикл (MAN) */
#define SUB_EXT    1                     /* внешний цикл (EXT) */
#define SUB_INT    2                     /* внутренний цикл (INT) */

/* --- Инверсия DIR (0/1), значение по умолчанию --- */
#ifndef AXIS_X_DIR_INVERT_DEFAULT
#define AXIS_X_DIR_INVERT_DEFAULT 1      /* 1 — реверс DIR оси X (джойстик) */
#endif
#ifndef AXIS_Z_DIR_INVERT_DEFAULT
#define AXIS_Z_DIR_INVERT_DEFAULT 0      /* 0 — DIR Z без инверсии */
#endif

/* --- FSM: коды ошибок и тип цикла --- */
#define ERR_INVALID_TRANSITION 1         /* недопустимый переход FSM */
#define ERR_LIMIT              2         /* срабатывание лимита */

#define CYCLE_EXT 0                      /* внешний цикл обработки */
#define CYCLE_INT 1                      /* внутренний цикл обработки */

/* --- Планировщик --- */
#define BLOCK_BUFFER_SIZE 16             /* глубина очереди сегментов planner */

#if defined(__AVR__)
#include <util/atomic.h>
#else
#ifndef ATOMIC_BLOCK
#define ATOMIC_BLOCK(type) for (uint8_t _atomic_lock = 1; _atomic_lock; _atomic_lock = 0) /* PC stub */
#define ATOMIC_RESTORESTATE              /* PC stub: пустой restore флагов */
#endif
#endif

/* --- Компенсация люфта (ТЗ: выборка при смене направления, ENABLE_BACKLASH=0 откл.) --- */
#define ENABLE_BACKLASH         1        /* 1 — модуль люфта в сборке; 0 — выкл на этапе компиляции */
#define BACKLASH_DIR_POS        1        /* направление увеличения координаты */
#define BACKLASH_DIR_NEG        0        /* направление уменьшения координаты */
#define BACKLASH_REF_DIR_X      BACKLASH_DIR_NEG  /* направление стартовой выборки X */
#define BACKLASH_REF_DIR_Z      BACKLASH_DIR_NEG  /* направление стартовой выборки Z */
#define BACKLASH_FIRST_SKIP     0        /* первый ход: без выборки */
#define BACKLASH_FIRST_ALWAYS   1        /* первый ход после вкл. — всегда выборка (ТЗ) */
#define BACKLASH_FIRST_REF      2        /* первый ход: только по REF-направлению */
#define BACKLASH_FIRST_MOVE     BACKLASH_FIRST_ALWAYS  /* активная политика первого хода */
/* Дистанция выборки по умолчанию (импульсы); EEPROM переопределяет */
#define BACKLASH_X_STEPS_DEFAULT    1130 /* заводский люфт X, шаги (~0.03 мм при 100 µstep/мм) */
#define BACKLASH_Z_STEPS_DEFAULT    625  /* заводский люфт Z, шаги (~0.08 мм при 100 µstep/мм) */
#define BACKLASH_X_CENTIMM            34 /* запасной люфт X в сотых мм при steps=0 */
#define BACKLASH_Z_CENTIMM            46 /* запасной люфт Z в сотых мм при steps=0 */
#define BACKLASH_STEPS_MAX            5000 /* верхний предел шагов люфта в меню */
/* Вкл компенсации в рантайме (EEPROM $46 / меню BlEn); ENABLE_BACKLASH — компиляция */
#define BACKLASH_ENABLE_DEFAULT       1  /* 1 — BlEn вкл по умолчанию */
/* Автовыборка при включении (EEPROM $40 / меню BlAu) */
#define BACKLASH_AUTO_DEFAULT         0  /* 0 — BlAu выкл по умолчанию */
#define BACKLASH_AUTO_SPEED_DEFAULT   50 /* макс. скорость выборки BlSp, мм/мин */
#define BACKLASH_AUTO_SPEED_MIN       5  /* мин. допустимый BlSp, мм/мин */
#define BACKLASH_AUTO_SPEED_MAX       200 /* макс. допустимый BlSp, мм/мин */
#define BACKLASH_MIN_SPEED_DEFAULT    20 /* мин. скорость выборки BlMn, мм/мин */
#define BACKLASH_MIN_SPEED_MIN        5  /* нижний предел BlMn в меню, мм/мин */
#define BACKLASH_MIN_SPEED_MAX        1000 /* верхний предел BlMn в меню, мм/мин */

/* --- Единицы отображения координат (EEPROM $44 / меню CrdU) --- */
#define COORD_UNIT_STEPS              0  /* отображение в шагах */
#define COORD_UNIT_MM                 1  /* отображение в мм */
#define COORD_UNIT_INCH               2  /* отображение в дюймах */
#define COORD_UNIT_DEFAULT            COORD_UNIT_MM  /* заводская единица CrdU */

/* --- Телеметрия --- */
#define TELEMETRY_BUFFER_SIZE 64         /* размер буфера телеметрии, байт */

#endif
