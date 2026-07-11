/*
 * ELS AlexA V2.1 — этап 2.2f: API шпинделя, потенциометр по режимам
 * Arduino Mega 2560, пины из wokwi diagram.json
 */

#include "src/config/config.h"
#include "src/config/config_storage.h"
#include "src/core/debug/debug_serial.h"
#include "src/core/debug/debug_trace.h"
#include "src/core/debug/serial_config.h"
#include "src/core/hal/hal_init.h"
#include "src/core/hal/hal_interrupts.h"
#include "src/core/hal/hal_timers.h"
#include "src/core/fsm/fsm_manager.h"
#include "src/core/motion/limits.h"
#include "src/core/motion/planner.h"
#include "src/core/motion/backlash.h"
#include "src/core/motion/motion_control.h"
#include "src/core/motion/motion_jog.h"
#include "src/core/motion/stepper_gen.h"
#include "src/core/process/estop_control.h"
#include "src/core/process/spindle_control.h"
#include "src/core/ui/ui_buttons.h"
#include "src/core/ui/ui_encoder.h"
#include "src/core/ui/ui_lcd.h"
#include "src/core/ui/ui_menu.h"
#include "src/core/ui/ui_pot.h"
#include "src/core/ui/ui_switches.h"
#include "src/modes/mode_divider/mode_divider.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <avr/wdt.h>

static unsigned long last_lcd_ms = 0;
static unsigned long last_pot_ms = 0;
static uint8_t startup_armed = 0;
static uint8_t lcd_dirty = 1;       /* 1 — буфер не совпадает с экраном / первый кадр */
static uint8_t lcd_menu_was = 0;    /* предыдущее ui_menu_is_active() */

#if DEBUG_ENABLED
/* Расшифровка MCUSR ATmega2560: POR/EXT/BOR/WDR/JTG */
static void debug_print_reset_cause(uint8_t mcusr) {
    debug_serial_print("RST MCUSR=");
    Serial.println(mcusr);
    debug_serial_print("RST flg:");
    if (mcusr == 0U) {
        debug_serial_println(" none");
        return;
    }
    if (mcusr & _BV(PORF)) Serial.print(F(" POR"));
    if (mcusr & _BV(EXTRF)) Serial.print(F(" EXT"));
    if (mcusr & _BV(BORF)) Serial.print(F(" BOR"));
    if (mcusr & _BV(WDRF)) Serial.print(F(" WDR"));
    if (mcusr & _BV(JTRF)) Serial.print(F(" JTG"));
    Serial.println();
}
#endif

/* Снимок полей главного экрана для cheap dirty-check (без snprintf). */
typedef struct {
    uint8_t startup_busy;
    uint8_t fsm_mode;
    uint8_t sw_mode;
    uint8_t sw_submode;
    uint8_t mpg_axis;
    int32_t pos_x;
    int32_t pos_z;
    char mark_x;
    char mark_z;
    long hand;
    uint16_t pot_filtered;
} LcdMainCache_t;

static LcdMainCache_t lcd_cache;
static char lcd_line[LCD_COLS + 1];
static char lcd_xf[11];
static char lcd_zf[11];
static char lcd_mpg[LCD_COLS + 1];

#define SETUP_MARK(tag) DBG_INFO("SYS", "SETUP", tag)

static const char *submode_short(uint8_t sub) {
    if (sub == SUB_EXT) return "Ext";
    if (sub == SUB_INT) return "Int";
    return "Man";
}

static void lcd_format_status_line(const char *mode, const char *feed, const char *sub,
                                   char *buf, size_t len, int8_t center_shift) {
    (void)len;
    memset(buf, ' ', LCD_COLS);

    size_t fl = strlen(feed);
    if (fl > LCD_COLS) fl = LCD_COLS;
    uint8_t fp = (uint8_t)((LCD_COLS - fl) / 2U);
    if (center_shift > 0) {
        uint8_t sh = (uint8_t)center_shift;
        if (fp + sh + fl <= LCD_COLS) {
            fp += sh;
        }
    }
    memcpy(buf + fp, feed, fl);

    size_t ml = strlen(mode);
    uint8_t max_ml = fp > 0U ? fp : 1U;
    if (ml > max_ml) ml = max_ml;
    memcpy(buf, mode, ml);

    size_t sl = strlen(sub);
    if (sl > 3U) sl = 3U;
    memcpy(buf + LCD_COLS - 3U, sub, sl);

    buf[LCD_COLS] = 0;
}

static void lcd_trim_decimal_leading_zeros(char *raw) {
    char *dot = strchr(raw, '.');
    if (dot == NULL || dot <= raw) {
        return;
    }
    char *p = raw;
    while (p < dot - 1 && *p == '0') {
        p++;
    }
    if (p == dot) {
        p--;
    }
    if (p > raw) {
        memmove(raw, p, strlen(p) + 1U);
    }
}

static void lcd_format_decimal_num(char *dst, uint32_t whole, uint32_t frac) {
    char raw[12];
    snprintf(raw, sizeof(raw), "%lu.%03lu", (unsigned long)whole, (unsigned long)(frac % 1000U));
    lcd_trim_decimal_leading_zeros(raw);

    size_t n = strlen(raw);
    memset(dst, ' ', 7);
    if (n >= 7U) {
        memcpy(dst, raw + n - 7U, 7);
    } else {
        memcpy(dst + 7U - n, raw, n);
    }
    dst[7] = 0;
}

static void lcd_steps_to_parts(int32_t steps, uint8_t axis, uint8_t units,
                              uint32_t *whole, uint32_t *frac, int8_t *neg) {
    uint32_t abs_s = (steps < 0) ? (uint32_t)(-steps) : (uint32_t)steps;
    uint16_t ms = config_get_motor_steps(axis);
    uint8_t ustep = config_get_microstep(axis);
    uint16_t pitch = config_get_screw_pitch(axis);
    uint32_t den;
    uint32_t val1000;

    *neg = (steps < 0) ? -1 : 1;
    *whole = 0;
    *frac = 0;
    if (abs_s == 0U || ms == 0U || ustep == 0U || pitch == 0U) {
        return;
    }

    den = (uint32_t)ms * (uint32_t)ustep * 100UL;
    if (den == 0U) {
        return;
    }

    val1000 = (abs_s / den) * (uint32_t)pitch * 1000UL;
    val1000 += (((uint32_t)(abs_s % den) * (uint32_t)pitch * 1000UL) / den);
    if (units == COORD_UNIT_INCH) {
        val1000 = (val1000 * 10000UL + 127000UL) / 254000UL;
    }
    *whole = val1000 / 1000UL;
    *frac = val1000 % 1000UL;
}

static void lcd_format_axis_field(char *dst, char axis, int32_t steps, char mark) {
    uint8_t units = config_get_coord_units();
    uint8_t axis_id = (axis == 'X') ? AXIS_X : AXIS_Z;

    if (units == COORD_UNIT_STEPS) {
        if (steps < 0) {
            snprintf(dst, 11, "%c-%07ld%c", axis, -(long)steps, mark);
        } else {
            snprintf(dst, 11, "%c %07ld%c", axis, (long)steps, mark);
        }
        return;
    }

    uint32_t whole;
    uint32_t frac;
    int8_t neg;
    char num[8];

    lcd_steps_to_parts(steps, axis_id, units, &whole, &frac, &neg);
    lcd_format_decimal_num(num, whole, frac);

    dst[0] = axis;
    dst[1] = (neg < 0) ? '-' : ' ';
    memcpy(dst + 2, num, 7);
    dst[9] = mark;
    dst[10] = 0;
}

static void lcd_format_mpg_line(char axis, long hand) {
    int n;

    memset(lcd_mpg, ' ', LCD_COLS);
    n = snprintf(lcd_mpg, LCD_COLS, "MPG %c %+ld", axis, hand);
    if (n < 0) {
        n = 0;
    }
    if ((uint8_t)n < LCD_COLS) {
        lcd_mpg[n] = ' ';
    }
    lcd_mpg[LCD_COLS - 1] = config_coord_unit_flag();
    lcd_mpg[LCD_COLS] = 0;
}

static void lcd_format_coords_line(char *buf, size_t len) {
    int32_t xs = motion_get_pos_steps(AXIS_X);
    int32_t zs = motion_get_pos_steps(AXIS_Z);
    lcd_format_axis_field(lcd_xf, 'X', xs, limits_lcd_marker(AXIS_X));
    lcd_format_axis_field(lcd_zf, 'Z', zs, limits_lcd_marker(AXIS_Z));
    snprintf(buf, len, "%s%s", lcd_xf, lcd_zf);
}

static char lcd_feed_txt[16];
static char lcd_div_angle[8];
static char lcd_div_line1[LCD_COLS + 1];
static char lcd_div_line2[LCD_COLS + 1];

/* LCD режима делительной головки: угол по центру стр.0, A/B/R и COU. */
static void update_divider_lcd(void) {
    if (planner_startup_busy()) {
        ui_lcd_set_line(0, "BL takeup...        ");
        memset(lcd_line, ' ', LCD_COLS);
        memcpy(lcd_line, "Wait", 4);
        lcd_line[LCD_COLS - 1] = config_coord_unit_flag();
        lcd_line[LCD_COLS] = 0;
        ui_lcd_set_line_raw(1, lcd_line);
        mode_divider_format_line2(lcd_div_line2, sizeof(lcd_div_line2));
        ui_lcd_set_line(2, lcd_div_line2);
        lcd_format_coords_line(lcd_line, sizeof(lcd_line));
        ui_lcd_set_line_raw(3, lcd_line);
        ui_lcd_clear_cursor();
        return;
    }

    SwitchState_t sw = ui_switches_get_state();

    mode_divider_format_angle_center(lcd_div_angle, sizeof(lcd_div_angle));
    lcd_format_status_line(ui_switches_get_mode_name(sw.mode),
                           lcd_div_angle,
                           submode_short(sw.submode),
                           lcd_line, sizeof(lcd_line), 1);
    ui_lcd_set_line(0, lcd_line);

    mode_divider_format_line1(lcd_div_line1, sizeof(lcd_div_line1));
    ui_lcd_set_line(1, lcd_div_line1);
    mode_divider_format_line2(lcd_div_line2, sizeof(lcd_div_line2));
    ui_lcd_set_line(2, lcd_div_line2);

    lcd_format_coords_line(lcd_line, sizeof(lcd_line));
    ui_lcd_set_line_raw(3, lcd_line);
    ui_lcd_clear_cursor();
}

static void update_main_lcd(void) {
    if (planner_startup_busy()) {
        ui_lcd_set_line(0, "BL takeup...        ");
        memset(lcd_line, ' ', LCD_COLS);
        memcpy(lcd_line, "Wait", 4);
        lcd_line[LCD_COLS - 1] = config_coord_unit_flag();
        lcd_line[LCD_COLS] = 0;
        ui_lcd_set_line_raw(1, lcd_line);
        ui_lcd_clear_line(2);
        lcd_format_coords_line(lcd_line, sizeof(lcd_line));
        ui_lcd_set_line_raw(3, lcd_line);
        ui_lcd_clear_cursor();
        return;
    }

    SwitchState_t sw = ui_switches_get_state();
    uint8_t mode = fsm_manager_get_mode();

    ui_pot_feed_format(lcd_feed_txt, sizeof(lcd_feed_txt), mode);
    lcd_format_status_line(ui_switches_get_mode_name(sw.mode),
                           lcd_feed_txt,
                           submode_short(sw.submode),
                           lcd_line, sizeof(lcd_line), 0);
    ui_lcd_set_line(0, lcd_line);

    char axis = (sw.mpg_axis == AXIS_X) ? 'X' : 'Z';
    lcd_format_mpg_line(axis, (long)motion_jog_get_hand(sw.mpg_axis));
    ui_lcd_set_line_raw(1, lcd_mpg);
    ui_lcd_clear_line(2);

    lcd_format_coords_line(lcd_line, sizeof(lcd_line));
    ui_lcd_set_line_raw(3, lcd_line);
    ui_lcd_clear_cursor();
}

static void update_lcd(void) {
    if (ui_menu_is_active()) {
        ui_menu_lcd();
    } else if (fsm_manager_get_mode() == 6U) {
        update_divider_lcd();
    } else {
        update_main_lcd();
    }
}

/* Сохранить снимок главного экрана после форматирования. */
static void lcd_cache_save_main(void) {
    SwitchState_t sw = ui_switches_get_state();

    lcd_cache.startup_busy = planner_startup_busy();
    lcd_cache.fsm_mode = fsm_manager_get_mode();
    lcd_cache.sw_mode = sw.mode;
    lcd_cache.sw_submode = sw.submode;
    lcd_cache.mpg_axis = sw.mpg_axis;
    lcd_cache.pos_x = motion_get_pos_steps(AXIS_X);
    lcd_cache.pos_z = motion_get_pos_steps(AXIS_Z);
    lcd_cache.mark_x = limits_lcd_marker(AXIS_X);
    lcd_cache.mark_z = limits_lcd_marker(AXIS_Z);
    lcd_cache.hand = (long)motion_jog_get_hand(sw.mpg_axis);
    lcd_cache.pot_filtered = ui_pot_get_value();
}

/* Дешёвая проверка: форматировать только при изменении данных LCD. */
static void lcd_mark_dirty_if_changed(void) {
    uint8_t menu = ui_menu_is_active();

    if (menu != lcd_menu_was) {
        lcd_dirty = 1;
        lcd_menu_was = menu;
    }
    if (menu) {
        lcd_dirty = 1;
        return;
    }

    SwitchState_t sw = ui_switches_get_state();
    uint8_t startup = planner_startup_busy();
    uint8_t mode = fsm_manager_get_mode();
    int32_t xs = motion_get_pos_steps(AXIS_X);
    int32_t zs = motion_get_pos_steps(AXIS_Z);
    char mx = limits_lcd_marker(AXIS_X);
    char mz = limits_lcd_marker(AXIS_Z);
    long hand = (long)motion_jog_get_hand(sw.mpg_axis);
    uint16_t pot = ui_pot_get_value();

    if (startup != lcd_cache.startup_busy ||
        mode != lcd_cache.fsm_mode ||
        sw.mode != lcd_cache.sw_mode ||
        sw.submode != lcd_cache.sw_submode ||
        sw.mpg_axis != lcd_cache.mpg_axis ||
        xs != lcd_cache.pos_x ||
        zs != lcd_cache.pos_z ||
        mx != lcd_cache.mark_x ||
        mz != lcd_cache.mark_z ||
        hand != lcd_cache.hand ||
        pot != lcd_cache.pot_filtered) {
        lcd_dirty = 1;
    }

    if (mode == 6U && mode_divider_lcd_dirty_poll()) {
        lcd_dirty = 1;
    }
}

void setup() {
    hal_init();
    debug_serial_init(SERIAL_BAUD);
    trace_crash_init();
    {
        uint8_t mcusr = MCUSR;
        MCUSR = 0;
#if DEBUG_ENABLED
        debug_print_reset_cause(mcusr);
#endif
    }
    serial_config_init();

#if DEBUG_ENABLED
    debug_serial_println("");
    debug_serial_println(FIRMWARE_NAME " " FIRMWARE_STAGE " Starting...");
#endif

    SETUP_MARK("cfg");
    config_load();
    SETUP_MARK("mot");
    motion_init();
    SETUP_MARK("jog");
    motion_jog_init();

    SETUP_MARK("lcd");
    ui_lcd_init();
    SETUP_MARK("sw");
    ui_switches_init();
    SETUP_MARK("btn");
    ui_buttons_init();
    SETUP_MARK("lim");
    limits_ui_init();
    SETUP_MARK("pot");
    ui_pot_init();
    SETUP_MARK("enc");
    ui_encoder_init();
    SETUP_MARK("menu");
    ui_menu_init();
    SETUP_MARK("fsm");
    fsm_manager_init();
    SETUP_MARK("spd");
    spindle_init();
    SETUP_MARK("isr");
    encoder_interrupts_init();

    SETUP_MARK("draw");
    update_lcd();
    ui_lcd_update();
    lcd_cache_save_main();
    lcd_dirty = 0;

    SETUP_MARK("t1");
    timer1_init(STEP_ISR_PERIOD_US);

    DBG_INFO("SYS", "INIT", "Stage 2.2f ready");
}

void loop() {
    if (!startup_armed) {
        startup_armed = 1;
        planner_startup_backlash_queue();
    }

    estop_check();
    ui_buttons_poll();
    ui_switches_poll();
    fsm_manager_poll();
    ui_encoder_poll();
    ui_menu_poll();
    fsm_manager_process();
    spindle_poll();
    serial_config_poll();

    if (millis() - last_pot_ms >= POT_READ_MS) {
        if (ui_pot_poll()) {
            DBG_INFO_VAL("UI", "POT", "pct", ui_pot_get_percent());
        }
        last_pot_ms = millis();
    }

    lcd_mark_dirty_if_changed();

    if (millis() - last_lcd_ms >= LCD_UPDATE_MS) {
        if (lcd_dirty) {
            update_lcd();
            if (!ui_menu_is_active()) {
                lcd_cache_save_main();
            }
            lcd_dirty = 0;
            ui_lcd_update();
        }
        last_lcd_ms = millis();
    }
}
