#ifndef MODE_GRIND_CFG_H
#define MODE_GRIND_CFG_H

/* ============================================================
КОНФИГУРАЦИЯ РЕЖИМА ШЛИФОВКИ
============================================================ */

/* --- Диапазоны параметров --- */
#define GRIND_MIN_DEPTH 0.001f           /* мин. глубина съёма, мм */
#define GRIND_MAX_DEPTH 0.100f           /* макс. глубина съёма, мм */
#define GRIND_DEFAULT_DEPTH 0.010f       /* глубина съёма по умолчанию, мм */

#define GRIND_MIN_PASSES 1               /* мин. число проходов */
#define GRIND_MAX_PASSES 20              /* макс. число проходов */
#define GRIND_DEFAULT_PASSES 5           /* проходов по умолчанию */

/* --- Скорости --- */
#define GRIND_MAX_FEED 500.0f            /* макс. подача шлифовки, мм/мин */
#define GRIND_MIN_FEED 10.0f             /* мин. подача шлифовки, мм/мин */

/* --- Безопасность --- */
#define GRIND_MAX_RPM 3000               /* макс. обороты шпинделя, об/мин */

#endif
