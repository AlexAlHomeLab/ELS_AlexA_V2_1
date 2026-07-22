#ifndef CONFIG_MPG_H
#define CONFIG_MPG_H

/* Режим Rapid + РГИ (только MPG; джойстик Rapid не меняется).
 * Скорости РГИ по осям — config_machine.h (MPG_SPEED_*). */

#define MPG_RAPID_MODE_LIVE       0   /* 0.1 мм/тик + движение сразу (как раньше) */
#define MPG_RAPID_MODE_APPROACH   1   /* точный подвод: шаг с селектора; цель при Rapid, ход при отпускании */

#ifndef MPG_RAPID_MODE
#define MPG_RAPID_MODE  MPG_RAPID_MODE_APPROACH
#endif

#if (MPG_RAPID_MODE != MPG_RAPID_MODE_LIVE) && (MPG_RAPID_MODE != MPG_RAPID_MODE_APPROACH)
#error "MPG_RAPID_MODE: только MPG_RAPID_MODE_LIVE или MPG_RAPID_MODE_APPROACH"
#endif

#endif
