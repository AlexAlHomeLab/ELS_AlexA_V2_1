#ifndef CONFIG_LCD_H
#define CONFIG_LCD_H

/* Выбор драйвера HD44780: раскомментировать ОДИН вариант.
 * По умолчанию — стандартная LiquidCrystal (минимум RAM). */

#define USE_LCD_SAFE_ASYNC
// #define USE_LCD_STANDARD

#if !defined(USE_LCD_SAFE_ASYNC) && !defined(USE_LCD_STANDARD)
#define USE_LCD_STANDARD
#endif

#if defined(USE_LCD_SAFE_ASYNC) && defined(USE_LCD_STANDARD)
#error "Только одна библиотека: USE_LCD_SAFE_ASYNC или USE_LCD_STANDARD"
#endif

#if defined(USE_LCD_SAFE_ASYNC)
#define LCD_SAFE_MAX_RETRIES    3
#define LCD_PROCESS_US_BUDGET   500    /* мкс за вызов ui_lcd_process_queue в loop */
#define LCD_FLUSH_US_BUDGET     15000  /* мкс на drain после ui_lcd_update / init */
#define LCD_SYNC_INTERVAL_MS    0     /* 0=выкл; >0 период syncAll() при EMI */
#endif

#endif
