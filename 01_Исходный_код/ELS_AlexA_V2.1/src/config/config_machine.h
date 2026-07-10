#ifndef CONFIG_MACHINE_H
#define CONFIG_MACHINE_H

/* Параметры станка: шаги/мм, скорости, шпиндель, EEPROM $32..$55.
 * screw_pitch хранится ×100 (сотые мм). */

#include "config_defs.h"

/* --- Заводские параметры оси X --- */
#define AXIS_X_MOTOR_STEPS_DEFAULT      200   /* полных шагов двигателя */
#define AXIS_X_MICROSTEP_DEFAULT        2     /* делитель микрошага */
#define AXIS_X_SCREW_PITCH_DEFAULT      100   /* x100: 0.42 мм */
#define AXIS_X_MAX_SPEED_DEFAULT        400   /* мм/мин */
#define AXIS_X_RAPID_SPEED_DEFAULT      1500  /* мм/мин, rapid джойстик */
#define AXIS_X_FEED_ACCEL_DEFAULT       3     /* уровень ускорения (×50 мм/с²) */

/* --- Заводские параметры оси Z --- */
#define AXIS_Z_MOTOR_STEPS_DEFAULT      200
#define AXIS_Z_MICROSTEP_DEFAULT        2
#define AXIS_Z_SCREW_PITCH_DEFAULT      100   /* x100: 1.60 мм */
#define AXIS_Z_MAX_SPEED_DEFAULT        400   /* мм/мин */
#define AXIS_Z_RAPID_SPEED_DEFAULT      1500  /* мм/мин */
#define AXIS_Z_FEED_ACCEL_DEFAULT       3

/* --- Шпиндель --- */
#define SPINDLE_PPR_DEFAULT             3000  /* импульсов энкодера на оборот */

/* --- Торможение джойстика (GRBL-style) --- */
/* Jdec: мин. шаги master-оси при отпускании (0 = мгновенный стоп) */
/* фактическая дистанция = max(min, accel_distance(v,a)), cap JOG_DECEL_STEPS_MAX_RUN */
#define JOG_DECEL_STEPS_DEFAULT         0
#define JOG_DECEL_STEPS_MIN             0
#define JOG_DECEL_STEPS_MAX             2000
#define JOG_DECEL_STEPS_MAX_RUN         200

/* --- Допустимые диапазоны (меню / EEPROM) --- */
#define AXIS_MOTOR_STEPS_MIN            50
#define AXIS_MOTOR_STEPS_MAX            2000
#define AXIS_MICROSTEP_MIN              1
#define AXIS_MICROSTEP_MAX              32
#define AXIS_SCREW_PITCH_MIN            10    /* x100: 0.10 мм */
#define AXIS_SCREW_PITCH_MAX            1000  /* x100: 10.00 мм */
#define AXIS_MAX_SPEED_MIN              10
#define AXIS_MAX_SPEED_MAX              5000
#define AXIS_RAPID_SPEED_MIN            10
#define AXIS_RAPID_SPEED_MAX            10000
#define AXIS_FEED_ACCEL_MIN             1
#define AXIS_FEED_ACCEL_MAX             20
#define SPINDLE_PPR_MIN                 100
#define SPINDLE_PPR_MAX                 10000

#ifdef __cplusplus
extern "C" {
#endif

void config_machine_load(void);  /* чтение EEPROM или defaults */
void config_machine_save(void);

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
