#ifndef MODE_GRIND_CFG_H
#define MODE_GRIND_CFG_H

/* ============================================================
КОНФИГУРАЦИЯ РЕЖИМА ШЛИФОВКИ
============================================================ */

/* --- Диапазоны параметров --- */
#define GRIND_MIN_DEPTH 0.001f
#define GRIND_MAX_DEPTH 0.100f
#define GRIND_DEFAULT_DEPTH 0.010f

#define GRIND_MIN_PASSES 1
#define GRIND_MAX_PASSES 20
#define GRIND_DEFAULT_PASSES 5

/* --- Скорости --- */
#define GRIND_MAX_FEED 500.0f
#define GRIND_MIN_FEED 10.0f

/* --- Безопасность --- */
#define GRIND_MAX_RPM 3000

#endif