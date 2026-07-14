#ifndef CONFIG_LCD_H
#define CONFIG_LCD_H

/* Выбор драйвера HD44780: раскомментировать ОДИН вариант.
 * По умолчанию — стандартная LiquidCrystal (минимум RAM). */

// #define USE_LCD_SAFE_ASYNC           /* LiquidCrystalSafeAsync + очередь из loop */
#define USE_LCD_STANDARD                 /* стандартная LiquidCrystal (блокирующий I/O) */

#if !defined(USE_LCD_SAFE_ASYNC) && !defined(USE_LCD_STANDARD)
#define USE_LCD_STANDARD                 /* fallback, если оба варианта закомментированы */
#endif

#if defined(USE_LCD_SAFE_ASYNC) && defined(USE_LCD_STANDARD)
#error "Только одна библиотека: USE_LCD_SAFE_ASYNC или USE_LCD_STANDARD"
#endif

/* Ширина шины HD44780 (только USE_LCD_STANDARD): раскомментировать ОДИН.
 * 8 бит — пины D0..D7 из hal_pins.h; 4 бит — только D4..D7. */
#define LCD_BUS_8BIT                     /* 8-битная шина данных LCD */
// #define LCD_BUS_4BIT                  /* 4-битная шина (D4..D7) */

#if !defined(LCD_BUS_8BIT) && !defined(LCD_BUS_4BIT)
#define LCD_BUS_4BIT                     /* fallback: 4 бита, если оба выкл. */
#endif

#if defined(LCD_BUS_8BIT) && defined(LCD_BUS_4BIT)
#error "Только одна шина: LCD_BUS_8BIT или LCD_BUS_4BIT"
#endif

#if defined(USE_LCD_SAFE_ASYNC) && defined(LCD_BUS_8BIT)
#error "LCD_BUS_8BIT поддерживается только с USE_LCD_STANDARD"
#endif

#if defined(USE_LCD_SAFE_ASYNC)
#define LCD_SAFE_MAX_RETRIES    3        /* повторы записи при сбое SafeAsync */
#define LCD_PROCESS_US_BUDGET   500      /* мкс за вызов ui_lcd_process_queue в loop */
#define LCD_FLUSH_US_BUDGET     15000    /* мкс на drain после ui_lcd_update / init */
#define LCD_SYNC_INTERVAL_MS    0        /* 0=выкл; >0 период syncAll() при EMI */
#endif

#endif
