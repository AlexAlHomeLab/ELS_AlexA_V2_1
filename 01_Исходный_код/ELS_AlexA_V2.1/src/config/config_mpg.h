#ifndef CONFIG_MPG_H
#define CONFIG_MPG_H

/* Режим Rapid + РГИ (только MPG; джойстик Rapid не меняется). */
#define MPG_RAPID_MODE_LIVE       0   /* 0.1 мм/тик + движение сразу (как раньше) */
#define MPG_RAPID_MODE_APPROACH   1   /* точный подвод: шаг с селектора; цель при Rapid, ход при отпускании */

#ifndef MPG_RAPID_MODE
#define MPG_RAPID_MODE  MPG_RAPID_MODE_APPROACH
#endif

#if (MPG_RAPID_MODE != MPG_RAPID_MODE_LIVE) && (MPG_RAPID_MODE != MPG_RAPID_MODE_APPROACH)
#error "MPG_RAPID_MODE: только MPG_RAPID_MODE_LIVE или MPG_RAPID_MODE_APPROACH"
#endif

/* Полярность РГИ по оси Z: 1 — вправо +Z, влево −Z (см. mpg_adjust_tick_sign) */
#ifndef MPG_AXIS_Z_INVERT
#define MPG_AXIS_Z_INVERT  1
#endif

/* Скорости РГИ, мм/мин — отдельно от джойстика/pot; в рантайме clamp к max_speed оси. */
#define MPG_SPEED_X1_X_MM_MIN           800   /* 1 шаг/тик, без Rapid */
#define MPG_SPEED_X1_Z_MM_MIN          2000
#define MPG_SPEED_001_X_MM_MIN          200   /* 0.01 мм/тик */
#define MPG_SPEED_001_Z_MM_MIN          400
#define MPG_SPEED_01_LIVE_X_MM_MIN      600   /* Rapid 0.1 мм/тик, LIVE */
#define MPG_SPEED_01_LIVE_Z_MM_MIN     2000
#define MPG_SPEED_APPROACH_X_MM_MIN     100   /* подвод при отпускании Rapid (APPROACH) */
#define MPG_SPEED_APPROACH_Z_MM_MIN     200

#endif

