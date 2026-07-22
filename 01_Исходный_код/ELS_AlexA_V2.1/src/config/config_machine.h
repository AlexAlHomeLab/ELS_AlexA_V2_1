#ifndef CONFIG_MACHINE_H
#define CONFIG_MACHINE_H

/* Движение по осям: кинематика, скорости, DIR, люфт (завод), РГИ (compile-time).
 * EEPROM machine $32..$55; рантайм люфта — config_backlash (BlX/BlZ, BlEn…).
 * screw_pitch хранится ×100 (сотые мм). */

#include "config_defs.h"
/* ========== Ось X ========== */
/* Кинематика (EEPROM) */
#define AXIS_X_MOTOR_STEPS_DEFAULT      1500   /* полных шагов двигателя на оборот */
#define AXIS_X_MICROSTEP_DEFAULT        1      /* делитель микрошага драйвера */
#define AXIS_X_SCREW_PITCH_DEFAULT      100    /* шаг винта ×100: 1.00 мм */
/* Скорости и ускорение (EEPROM) */
#define AXIS_X_MAX_SPEED_DEFAULT        120   /* макс. рабочая подача, мм/мин */
#define AXIS_X_RAPID_SPEED_DEFAULT      130   /* rapid джойстика, мм/мин */
#define AXIS_X_FEED_ACCEL_DEFAULT       2     /* уровень ускорения (×50 мм/с²) */
/* Направление STEP / джойстик (EEPROM) */
#ifndef AXIS_X_DIR_INVERT_DEFAULT
#define AXIS_X_DIR_INVERT_DEFAULT       1      /* 1 — реверс DIR оси X */
#endif
/* Люфт: заводские шаги / сотые мм (EEPROM BlX; рантайм config_backlash) */
#define BACKLASH_X_STEPS_DEFAULT        520   /* ~0.03 мм при 100 шаг/мм */
#define BACKLASH_X_CENTIMM              34     /* запас при steps=0, сотые мм */
#define BACKLASH_REF_DIR_X              BACKLASH_DIR_NEG  /* стартовая выборка */
/* РГИ, мм/мин (прошивка; clamp к max_speed оси) */
#define MPG_SPEED_X1_X_MM_MIN           12     /* 1 шаг/тик */
#define MPG_SPEED_001_X_MM_MIN          20     /* 0.01 мм/тик */
#define MPG_SPEED_01_LIVE_X_MM_MIN      30     /* Rapid 0.1 мм/тик, LIVE */
#define MPG_SPEED_APPROACH_X_MM_MIN     100    /* подвод Rapid (APPROACH) */

/* ========== Ось Z ========== */
/* Кинематика (EEPROM) */
#define AXIS_Z_MOTOR_STEPS_DEFAULT      2000   /* полных шагов двигателя на оборот */
#define AXIS_Z_MICROSTEP_DEFAULT        1      /* делитель микрошага драйвера */
#define AXIS_Z_SCREW_PITCH_DEFAULT      200    /* шаг винта ×100: 2.00 мм */
/* Скорости и ускорение (EEPROM) */
#define AXIS_Z_MAX_SPEED_DEFAULT        1200   /* макс. рабочая подача, мм/мин */
#define AXIS_Z_RAPID_SPEED_DEFAULT      1300   /* rapid джойстика, мм/мин */
#define AXIS_Z_FEED_ACCEL_DEFAULT       10     /* уровень ускорения (×50 мм/с²) */
/* Направление STEP / джойстик (EEPROM) */
#ifndef AXIS_Z_DIR_INVERT_DEFAULT
#define AXIS_Z_DIR_INVERT_DEFAULT       0      /* 0 — DIR Z без инверсии */
#endif
/* Люфт (EEPROM BlZ) */
#define BACKLASH_Z_STEPS_DEFAULT        521    /* ~0.08 мм при 100 шаг/мм */
#define BACKLASH_Z_CENTIMM              46     /* запас при steps=0, сотые мм */
#define BACKLASH_REF_DIR_Z              BACKLASH_DIR_NEG
/* РГИ: полярность тиков (mpg_adjust_tick_sign); скорости мм/мин */
#ifndef MPG_AXIS_Z_INVERT
#define MPG_AXIS_Z_INVERT               1      /* 1 — вправо +Z, влево −Z */
#endif
#define MPG_SPEED_X1_Z_MM_MIN           200
#define MPG_SPEED_001_Z_MM_MIN          200
#define MPG_SPEED_01_LIVE_Z_MM_MIN      400
#define MPG_SPEED_APPROACH_Z_MM_MIN     200

/* ========== Джойстик (общее) ========== */
/* Jdec: мин. шаги master-оси при отпускании (0 = мгновенный стоп, EEPROM) */
/* фактическая дистанция = max(min, accel_distance(v,a)), cap JOG_DECEL_STEPS_MAX_RUN */
#define JOG_DECEL_STEPS_DEFAULT         0      /* заводской Jdec, шаги */
#define JOG_DECEL_STEPS_MIN             0      /* нижний предел Jdec в меню */
#define JOG_DECEL_STEPS_MAX             2000   /* верхний предел Jdec в меню */
#define JOG_DECEL_STEPS_MAX_RUN         200    /* потолок дистанции торможения в рантайме */
/* ========== Шпиндель / sync (не ось, EEPROM PPR) ========== */
#define SPINDLE_PPR_DEFAULT             3000   /* импульсов энкодера на оборот (PPR) */
/* ========== Лимиты меню / EEPROM (общие для осей) ========== */
#define AXIS_MOTOR_STEPS_MIN            200    /* мин. шагов двигателя на оборот */
#define AXIS_MOTOR_STEPS_MAX            10000  /* макс. шагов двигателя на оборот */
#define AXIS_MICROSTEP_MIN              1      /* мин. микрошаг */
#define AXIS_MICROSTEP_MAX              32     /* макс. микрошаг */
#define AXIS_SCREW_PITCH_MIN            10     /* мин. шаг винта ×100: 0.10 мм */
#define AXIS_SCREW_PITCH_MAX            2000   /* макс. шаг винта ×100: 10.00 мм */
#define AXIS_MAX_SPEED_MIN              10     /* мин. max_speed, мм/мин */
#define AXIS_MAX_SPEED_MAX              1200   /* макс. max_speed, мм/мин */
#define AXIS_RAPID_SPEED_MIN            10     /* мин. rapid, мм/мин */
#define AXIS_RAPID_SPEED_MAX            1200   /* макс. rapid, мм/мин */
#define AXIS_FEED_ACCEL_MIN             1      /* мин. уровень ускорения */
#define AXIS_FEED_ACCEL_MAX             20     /* макс. уровень ускорения */
#define SPINDLE_PPR_MIN                 10     /* мин. PPR энкодера шпинделя */
#define SPINDLE_PPR_MAX                 3000   /* макс. PPR энкодера шпинделя */
#ifdef __cplusplus

extern "C" {

#endif



void config_machine_load(void);  /* чтение EEPROM или defaults */
void config_machine_save(void);
void config_machine_factory_reset(void);

float config_get_steps_per_mm(uint8_t axis);  /* motor×micro / pitch_mm */
uint16_t config_get_motor_steps(uint8_t axis);
uint8_t config_get_microstep(uint8_t axis);
uint16_t config_get_screw_pitch(uint8_t axis);       /* ×100 мм */
uint16_t config_get_max_speed_mm_min(uint8_t axis);
uint16_t config_get_rapid_speed_mm_min(uint8_t axis);
uint8_t config_get_feed_accel(uint8_t axis);
uint8_t config_get_dir_invert(uint8_t axis);         /* 0/1 инверсия DIR */
uint16_t config_get_spindle_ppr(void);
uint16_t config_get_jog_decel_steps(void);
uint32_t config_mm_min_to_sps(uint8_t axis, float mm_min);  /* мм/мин → steps/s */

void config_set_motor_steps(uint8_t axis, uint16_t steps);
void config_set_microstep(uint8_t axis, uint8_t ms);
void config_set_screw_pitch(uint8_t axis, uint16_t pitch_cents);
void config_set_max_speed_mm_min(uint8_t axis, uint16_t mm_min);
void config_set_rapid_speed_mm_min(uint8_t axis, uint16_t mm_min);
void config_set_feed_accel(uint8_t axis, uint8_t accel);
void config_set_dir_invert(uint8_t axis, uint8_t invert);
void config_set_spindle_ppr(uint16_t ppr);
void config_set_jog_decel_steps(uint16_t steps);
#ifdef __cplusplus
}
#endif

#endif


