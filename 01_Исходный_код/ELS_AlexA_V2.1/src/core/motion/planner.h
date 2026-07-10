#ifndef PLANNER_H
#define PLANNER_H

/* Планировщик сегментов движения (кольцевой буфер BLOCK_BUFFER_SIZE).
 * Рассчитывает junction speeds, передаёт команды в stepper_gen (dds_motion_*). */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PLANNER_FLAG_BACKLASH  0x01U  /* блок — только выборка люфта */

/* Сегмент в очереди планировщика */
typedef struct {
    float target_x;           /* цель X, мм (отображение) */
    float target_z;           /* цель Z, мм */
    int32_t target_steps_x;   /* цель X, шаги (источник истины) */
    int32_t target_steps_z;   /* цель Z, шаги */
    float speed;              /* номинальная скорость, мм/мин */
    float entry_speed;        /* скорость входа в сегмент, мм/мин */
    float exit_speed;         /* скорость выхода, мм/мин */
    float distance;           /* длина сегмента master-оси, шаги */
    uint8_t direction_x;      /* 0/1 — знак ΔX */
    uint8_t direction_z;      /* 0/1 — знак ΔZ */
    uint8_t flags;            /* PLANNER_FLAG_* */
    uint8_t bl_axis;          /* ось выборки люфта */
    uint16_t bl_steps;        /* шаги выборки */
    uint8_t active;           /* 1 — блок занят в буфере */
} PlannerBlock_t;

/* --- Очередь сегментов --- */

int planner_add_move(float x, float z, float speed);  /* абс. координаты, мм */
int planner_add_move_steps(int32_t tx, int32_t tz, float speed);  /* 0 / -1 full / -2 zero */
int planner_add_backlash_takeup(uint8_t axis, float speed_mm_min);

void planner_startup_backlash_queue(void);  /* автовыборка при старте (EEPROM BlAu) */
uint8_t planner_startup_busy(void);         /* 1 пока не завершена стартовая выборка */

/* --- Немедленное исполнение (минуя буфер) --- */

void planner_exec_axis(uint8_t axis, int32_t target_steps, float speed_mm_min, uint8_t jog);
uint8_t planner_exec_jog(int32_t tx, int32_t tz, float speed_mm_min, const char *kind, uint8_t cruise,
                         uint8_t lim_hit, uint8_t lim_cmp, int32_t lim_cmp_stp);
void planner_jog_stop(void);
void planner_jog_halt(void);  /* атомарный стоп jog + сброс block_exec */

/* --- Обработка буфера --- */

void planner_calculate_junctions(void);  /* пересчёт entry/exit по цепочке блоков */
PlannerBlock_t* planner_get_next(void);  /* взять следующий блок (сдвиг tail) */
void planner_stop_all(void);           /* очистка буфера + dds_motion_stop */
void planner_process(void);            /* main loop: лог, завершение, старт блока */

uint8_t planner_is_empty(void);
uint8_t planner_is_busy(void);  /* exec || dds busy || очередь не пуста */

void planner_get_queue_target(float *x, float *z);
void planner_get_queue_target_steps(int32_t *tx, int32_t *tz);  /* цель последнего в очереди */

/* Зарезервировано (пока не используется) */
void planner_set_acceleration(float accel);
void planner_set_max_speed(float speed);

#ifdef __cplusplus
}
#endif

#endif
