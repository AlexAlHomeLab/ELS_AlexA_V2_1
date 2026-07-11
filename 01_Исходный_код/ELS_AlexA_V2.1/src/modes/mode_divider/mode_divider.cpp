#include "mode_divider.h"
#include "mode_divider_cfg.h"
#include "../../config/config.h"
#include "../../config/config_storage.h"
#include "../../core/debug/debug_serial.h"
#include "../../core/hal/hal_pins.h"
#include "../../core/process/spindle_control.h"
#include "../../core/ui/ui_buttons.h"
#include "../../core/ui/ui_menu.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint8_t mode_active = 0;
static uint16_t div_a = DIVIDER_DEFAULT_PARTS;   /* A — число частей деления */
static uint16_t div_b = 1;                       /* B — текущая часть (1…A) */
static int32_t div_zero_count = 0;               /* опорный счёт энкодера после Select */
static int32_t div_lcd_last_cnt = 0;             /* для dirty-check LCD */
static uint8_t div_lcd_force = 1;                /* принудительное обновление экрана */
static uint8_t div_r_was_zero = 0;               /* предыдущее состояние R≈0 для бипера */

#define DIV_R_ZERO_THRESH 0.01f   /* порог R=0, половина шага отображения 0.01° */
#define DIV_BEEP_SHORT_MS 40U
#define DIV_BEEP_LONG_MS  200U

/* Тиков на оборот шпинделя: PPR × 2 (квадратурный энкодер). */
static int32_t divider_ticks_per_rev(void) {
    return (int32_t)SPINDLE_ENCODER_PPR * 2;
}

/* Счёт энкодера относительно опорной точки (Select). */
static int32_t div_count_rel(void) {
    return spindle_get_count() - div_zero_count;
}

/* Фаза в пределах одного оборота [0 … TICKS_PER_REV). */
static int32_t div_mod_phase(int32_t rel) {
    int32_t tpr = divider_ticks_per_rev();
    if (tpr <= 0) {
        return 0;
    }
    int32_t t = rel % tpr;
    if (t < 0) {
        t += tpr;
    }
    return t;
}

/* Угол фазы 0…360° для строки 0 LCD. */
static float div_angle_deg(int32_t rel) {
    int32_t tpr = divider_ticks_per_rev();
    if (tpr <= 0) {
        return 0.0f;
    }
    int32_t t = div_mod_phase(rel);
    return (float)t * 360.0f / (float)tpr;
}

/* Целевой угол для части B (1-based). */
static float div_target_deg(uint16_t a, uint16_t b) {
    if (a == 0) {
        return 0.0f;
    }
    return 360.0f * (float)(b - 1U) / (float)a;
}

/* Нормализация R в диапазон (-180, +180]. */
static float div_normalize_r(float r) {
    while (r > 180.0f) {
        r -= 360.0f;
    }
    while (r <= -180.0f) {
        r += 360.0f;
    }
    return r;
}

static float div_r_deg(int32_t rel, uint16_t a, uint16_t b) {
    return div_normalize_r(div_target_deg(a, b) - div_angle_deg(rel));
}

static void div_mark_lcd_dirty(void) {
    div_lcd_force = 1;
}

static uint8_t div_r_is_zero(float r) {
    return fabsf(r) < DIV_R_ZERO_THRESH;
}

static void div_sync_r_zero_state(void) {
    div_r_was_zero = div_r_is_zero(div_r_deg(div_count_rel(), div_a, div_b));
}

static void div_buzzer_long(void) {
    if (!config_get_buzzer_on()) {
        return;
    }
    tone(BUZZER_PIN, 2500, DIV_BEEP_LONG_MS);
}

static void div_buzzer_double_short(void) {
    if (!config_get_buzzer_on()) {
        return;
    }
    tone(BUZZER_PIN, 2500, DIV_BEEP_SHORT_MS);
    delay(45);
    noTone(BUZZER_PIN);
    delay(80);
    tone(BUZZER_PIN, 2500, DIV_BEEP_SHORT_MS);
}

/* Бип при входе/выходе R из нуля: длинный — на ноль, двойной короткий — уход. */
static void div_poll_r_beep(void) {
    float r = div_r_deg(div_count_rel(), div_a, div_b);
    uint8_t is_zero = div_r_is_zero(r);

    if (is_zero && !div_r_was_zero) {
        div_buzzer_long();
        DBG_INFO("DIV", "R", "zero hit");
    } else if (!is_zero && div_r_was_zero) {
        div_buzzer_double_short();
        DBG_INFO("DIV", "R", "zero leave");
    }
    div_r_was_zero = is_zero;
}

void mode_divider_enter(void) {
    mode_active = 1;
    div_a = DIVIDER_DEFAULT_PARTS;
    div_b = 1;
    div_zero_count = spindle_get_count();
    div_lcd_last_cnt = spindle_get_count();
    div_lcd_force = 1;
    div_sync_r_zero_state();
    DBG_INFO_VAL("DIV", "ENTER", "a", div_a);
}

void mode_divider_exit(void) {
    mode_active = 0;
    div_lcd_force = 1;
    DBG_INFO("DIV", "EXIT", "ok");
}

/* Опрос кнопок A/B/Select и бипера R (только вне меню EEPROM). */
void mode_divider_process(void) {
    if (!mode_active || ui_menu_is_active()) {
        return;
    }

    div_poll_r_beep();

    ButtonClicks_t cl = ui_buttons_get_clicks();

    if (cl.up) {
        mode_divider_parts_up();
    }
    if (cl.down) {
        mode_divider_parts_down();
    }
    if (cl.left) {
        mode_divider_part_prev();
    }
    if (cl.right) {
        mode_divider_part_next();
    }
    if (cl.select) {
        mode_divider_zero_reset();
    }
}

void mode_divider_zero_reset(void) {
    div_zero_count = spindle_get_count();
    div_mark_lcd_dirty();
    div_sync_r_zero_state();
    DBG_INFO_VAL_I32("DIV", "ZERO", "cnt", div_zero_count);
}

void mode_divider_parts_up(void) {
    if (div_a < DIVIDER_MAX_PARTS) {
        div_a++;
    }
    if (div_b > div_a) {
        div_b = div_a;
    }
    div_mark_lcd_dirty();
    div_sync_r_zero_state();
    DBG_INFO_VAL("DIV", "A", "up", div_a);
}

void mode_divider_parts_down(void) {
    if (div_a > DIVIDER_MIN_PARTS) {
        div_a--;
    }
    if (div_b > div_a) {
        div_b = div_a;
    }
    div_mark_lcd_dirty();
    div_sync_r_zero_state();
    DBG_INFO_VAL("DIV", "A", "dn", div_a);
}

void mode_divider_part_prev(void) {
    if (div_b <= 1U) {
        div_b = div_a;
    } else {
        div_b--;
    }
    div_mark_lcd_dirty();
    div_sync_r_zero_state();
    DBG_INFO_VAL("DIV", "B", "prev", div_b);
}

void mode_divider_part_next(void) {
    if (div_b >= div_a) {
        div_b = 1;
    } else {
        div_b++;
    }
    div_mark_lcd_dirty();
    div_sync_r_zero_state();
    DBG_INFO_VAL("DIV", "B", "next", div_b);
}

uint16_t mode_divider_get_a(void) {
    return div_a;
}

uint16_t mode_divider_get_b(void) {
    return div_b;
}

float mode_divider_get_angle_deg(void) {
    return div_angle_deg(div_count_rel());
}

float mode_divider_get_r_deg(void) {
    return div_r_deg(div_count_rel(), div_a, div_b);
}

void mode_divider_format_angle_center(char *buf, size_t n) {
    float ang = mode_divider_get_angle_deg();
    unsigned iw = (unsigned)ang;
    unsigned frac = (unsigned)((ang - (float)iw) * 100.0f + 0.5f);
    if (frac >= 100U) {
        frac = 0;
        iw++;
    }
    if (iw >= 360U) {
        iw = 0;
    }
    snprintf(buf, n, "%03u.%02u", iw, frac % 100U);
}

void mode_divider_format_line1(char *buf, size_t n) {
    float r = mode_divider_get_r_deg();
    unsigned ri = (unsigned)fabsf(r);
    unsigned rf = (unsigned)(fabsf(r) * 100.0f + 0.5f) % 100U;

    snprintf(buf, n, "A %u B %u R%c%u.%02u",
             (unsigned)div_a, (unsigned)div_b,
             (r >= 0.0f) ? '+' : '-', ri, rf);
}

void mode_divider_format_line2(char *buf, size_t n) {
    int32_t rel = div_count_rel();
    int32_t tpr = divider_ticks_per_rev();
    int32_t revs = 0;
    int32_t phase4 = 0;

    if (tpr > 0) {
        revs = rel / tpr;
        int32_t phase = div_mod_phase(rel);
        phase4 = (phase * 10000L) / tpr;
    }

    snprintf(buf, n, "COU %c%07ld:%04ld",
             (revs < 0) ? '-' : '+',
             (long)((revs < 0) ? -revs : revs),
             (long)phase4);
}

uint8_t mode_divider_is_active(void) {
    return mode_active;
}

/* Проверка необходимости перерисовки LCD (вращение шпинделя или смена A/B). */
uint8_t mode_divider_lcd_dirty_poll(void) {
    if (!mode_active) {
        return 0;
    }
    int32_t cnt = spindle_get_count();
    if (div_lcd_force || cnt != div_lcd_last_cnt) {
        div_lcd_last_cnt = cnt;
        div_lcd_force = 0;
        return 1;
    }
    return 0;
}
