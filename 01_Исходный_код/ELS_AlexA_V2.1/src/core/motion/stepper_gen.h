#ifndef STEPPER_GEN_H
#define STEPPER_GEN_H

/* Генератор шагов DDS (Direct Digital Synthesis) и профиль движения осей X/Z.
 * Вызывается из ISR таймера; планировщик передаёт команды через MotionCommand_t. */

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Флаги команды движения (MotionCommand_t.flags) */
#define MOTION_FLAG_BACKLASH   0x01U  /* только выборка люфта по bl_axis */
#define MOTION_FLAG_JOG        0x02U  /* ручная подача / джог */
#define MOTION_FLAG_JOG_CRUISE 0x04U  /* джог без конечного торможения (круиз) */

/* Команда движения от планировщика */
typedef struct {
    int32_t target_x;       /* целевая позиция X, шаги */
    int32_t target_z;       /* целевая позиция Z, шаги */
    float nominal_mm_min;   /* номинальная скорость, мм/мин */
    float entry_mm_min;     /* скорость входа в профиль, мм/мин */
    float exit_mm_min;      /* скорость выхода из профиля, мм/мин */
    uint8_t flags;          /* MOTION_FLAG_* */
    uint8_t bl_axis;        /* ось для MOTION_FLAG_BACKLASH */
    uint16_t bl_steps;      /* шаги выборки люфта */
} MotionCommand_t;

/* Состояние одной оси (DDS-аккумулятор и цель) */
typedef struct {
    uint32_t accumulator;     /* DDS-накопитель (порог 0x80000000 = 1 шаг) */
    uint32_t step_increment;  /* приращение за тик ISR, steps/sec → DDS */
    int32_t position;         /* текущая позиция, шаги */
    int32_t target_position;  /* целевая позиция, шаги */
    uint8_t direction;        /* 0/1 — направление DIR */
    uint8_t enabled;          /* разрешение генерации шагов */
} AxisState_t;

/* --- Инициализация и низкоуровневый DDS --- */

void dds_init(void);  /* сброс осей, профиля и лога движения */

/* Пересчёт steps/sec в DDS-приращение для STEP_ISR_FREQ_HZ */
uint32_t dds_calc_increment(uint32_t steps_per_sec);

void dds_set_speed(uint8_t axis, uint32_t steps_per_sec);  /* axis: AXIS_X/AXIS_Z */
void dds_set_direction(uint8_t axis, uint8_t dir);           /* учёт config dir_invert */
void dds_enable(uint8_t axis, uint8_t enable);

/* Генерация шагов; вызывать из ISR таймера шагов */
void stepper_generate_steps(void);

int32_t dds_get_position(uint8_t axis);
void dds_set_position(uint8_t axis, int32_t pos);   /* позиция и цель = pos */
void dds_set_target(uint8_t axis, int32_t target);
int32_t dds_get_target(uint8_t axis);

/* 1 если позиция == цель и нет ожидающего люфта */
uint8_t dds_at_target(uint8_t axis);

uint8_t dds_get_direction(uint8_t axis);
void dds_reset_accumulator(void);  /* обнулить DDS-накопители обеих осей */

/* --- Профиль движения (планировщик) --- */

/* Старт движения: профиль разгона/торможения или jog cruise */
void dds_motion_start(const MotionCommand_t *cmd);

/* Смена цели во время jog; 1 при успехе */
uint8_t dds_motion_jog_retarget(const MotionCommand_t *cmd);

void dds_motion_jog_release(void);  /* эквивалент dds_motion_stop для jog */

void dds_motion_update_target(uint8_t axis, int32_t target);

/* Текущая скорость оси по профилю, мм/мин; 0 если движение неактивно */
float dds_motion_get_speed_mm_min(uint8_t axis);

uint8_t dds_motion_busy(void);              /* активен профиль движения */
uint8_t dds_motion_jog_cruise_active(void); /* активен режим jog cruise */
void dds_motion_stop(void);                 /* немедленная остановка профиля */

/* Выгрузка очереди лога движения в отладочный UART (main loop) */
void dds_motion_log_poll(void);

#ifdef __cplusplus
}
#endif

#endif
