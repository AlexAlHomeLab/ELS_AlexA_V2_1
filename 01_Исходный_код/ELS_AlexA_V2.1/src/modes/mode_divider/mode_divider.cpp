#include "mode_divider.h"
#include "mode_divider_cfg.h"
#include "../../config/config.h"
#include "../../core/debug/debug_serial.h"
#include "../../core/hal/hal_buzzer.h"
#include "../../core/hal/hal_pins.h"
#include "../../core/process/spindle_control.h"
#include "../../core/ui/ui_buttons.h"
#include "../../core/ui/ui_menu.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint8_t mode_active = 0;
static uint16_t div_a = DIVIDER_DEFAULT_PARTS;   /* A LCD: число делений */
static uint16_t div_b = 1;                       /* B LCD: текущее деление (1…div_a) */
static int32_t div_zero_count = 0;               /* опорный счёт энкодера после Select */
static int32_t div_lcd_last_cnt = 0;             /* для dirty-check LCD */
static uint8_t div_lcd_force = 1;                /* принудительное обновление экрана */
static uint8_t div_r_was_zero = 0;               /* предыдущее состояние R≈0 для бипера */

#define DIV_R_ZERO_THRESH 0.01f   /* порог R=0, половина шага отображения 0.01° */
#define DIV_BEEP_SHORT_MS 40U
#define DIV_BEEP_LONG_MS  200U

/* Тиков на оборот: PPR энкодера (1 канал), без удвоения квадратуры. */
static int32_t divider_ticks_per_rev(void) {
    return (int32_t)SPINDLE_ENCODER_PPR;
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

/* Целевой ход в тиках для части b (1…a): b/a оборота. */
static int32_t div_need_ticks(uint16_t a, uint16_t b) {
    int32_t tpr = divider_ticks_per_rev();
    if (a == 0 || tpr <= 0) {
        return 0;
    }
    return (int32_t)(((int64_t)tpr * (int64_t)b) / (int64_t)a);
}

/* R: разница target−rel; при проскоке знак «назад», иначе — по направлению вращения. */
static float div_r_deg(int32_t rel, uint16_t a, uint16_t b) {
    int32_t tpr = divider_ticks_per_rev();
    int32_t target = div_need_ticks(a, b);
    if (tpr <= 0 || target <= 0) {
        return 0.0f;
    }

    int32_t diff_ticks = target - rel;
    float mag = fabsf((float)diff_ticks * 360.0f / (float)tpr);

    if (mag < DIV_R_ZERO_THRESH) {
        return 0.0f;
    }

    if (diff_ticks < 0) {
        return -mag;
    }
    return (spindle_get_dir() == 0U) ? mag : -mag;
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
    hal_buzzer_beep_ms(DIV_BEEP_LONG_MS);
}

static void div_buzzer_double_short(void) {
    hal_buzzer_beep_double_ms(DIV_BEEP_SHORT_MS, 80);
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
    div_b = 1;
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
    if (ri >= 360U) {
        ri = 360U;
        rf = 0U;
    }

    /* A — число делений; B — текущее деление. */
    snprintf(buf, n, "A %u B %u R%c%u.%02u",
             (unsigned)div_a, (unsigned)div_b,
             (r >= 0.0f) ? '+' : '-', ri, rf);
}

void mode_divider_format_line2(char *buf, size_t n) {
    int32_t cnt = spindle_get_count();
    int32_t tpr = divider_ticks_per_rev();
    int32_t revs = 0;
    int32_t phase = 0;
    char sign = '+';

    if (tpr > 0) {
        if (cnt >= 0) {
            revs = cnt / tpr;
            phase = cnt % tpr;
        } else {
            int32_t ac = -cnt;
            revs = ac / tpr;
            phase = ac % tpr;
            sign = '-';
        }
    }

    snprintf(buf, n, "COU %c%07ld:%04ld", sign, (long)revs, (long)phase);
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
