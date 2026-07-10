#ifndef CONFIG_MODES_H
#define CONFIG_MODES_H

/* Включение режимов обработки на этапе сборки (0 = отключить модуль). */

#define ENABLE_MODE_ASYNC   1  /* асинхронная подача, джог, РГИ */
#define ENABLE_MODE_SYNC    1  /* синхронная подача, мм/об */
#define ENABLE_MODE_THREAD  1  /* нарезание резьбы */
#define ENABLE_MODE_CHAMFER 1  /* фаска */
#define ENABLE_MODE_CONE    1  /* конус */
#define ENABLE_MODE_SPHERE  1  /* сфера */
#define ENABLE_MODE_DIVIDER 1  /* делительная головка */
#define ENABLE_MODE_GRIND   1  /* шлифовка */

#endif
