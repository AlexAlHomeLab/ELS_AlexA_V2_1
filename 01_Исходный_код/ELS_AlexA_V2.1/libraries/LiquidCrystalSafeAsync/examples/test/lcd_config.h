#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

/*
 * Выбор библиотеки LCD (условная компиляция).
 *
 * Способ 1 — только для Arduino IDE (без PlatformIO):
 *   раскомментировать ОДИН дефайн ниже.
 * При сборке через PlatformIO дефайны здесь не менять — см. способ 2.
 */
// #define USE_LCD_SAFE_ASYNC
// #define USE_LCD_STANDARD

/*
 * Способ 2 — PlatformIO (из корня репозитория или из wokwi/):
 *   pio run -e megaatmega2560_debug        — LiquidCrystalSafeAsync
 *   pio run -e megaatmega2560_std_lcd      — стандартная LiquidCrystal
 */

#if !defined(USE_LCD_SAFE_ASYNC) && !defined(USE_LCD_STANDARD)
#define USE_LCD_SAFE_ASYNC
#endif

#if defined(USE_LCD_SAFE_ASYNC) && defined(USE_LCD_STANDARD)
#error "Выберите только одну библиотеку: USE_LCD_SAFE_ASYNC или USE_LCD_STANDARD"
#endif

#endif
