#include "ui_switches.h"
#include "ui_io.h"
#include "../motion/motion_jog.h"
#include "../motion/motor_en.h"
#include "../debug/debug_serial.h"
#include "../hal/hal_buzzer.h"
#include "../hal/hal_pins.h"
#include "../../config/config_board.h"
#include "../../config/config_defs.h"
#include <Arduino.h>
#include <EncButton.h>

static SwitchState_t sw_state;
static uint8_t latched_mode = 1;
static uint8_t latched_mpg_axis = AXIS_Z;
static uint8_t latched_mpg_scale = 0;
static uint8_t last_mode = 0;
static uint8_t latched_submode = SUB_MANUAL;
static uint8_t last_submode = 0xFF;
static uint8_t last_mpg_axis = 0xFF;
static uint8_t last_mpg_scale = 0xFF;
static uint8_t last_mode_off = 0xFF;  /* для лога перехода MODE_OFF */
static unsigned long mode_off_raw_ms; /* момент первого scan==0; 0 — не ждём */

static ButtonT<MODE_PIN_1> sw_mode_1;
static ButtonT<MODE_PIN_2> sw_mode_2;
static ButtonT<MODE_PIN_3> sw_mode_3;
static ButtonT<MODE_PIN_4> sw_mode_4;
static ButtonT<MODE_PIN_5> sw_mode_5;
static ButtonT<MODE_PIN_6> sw_mode_6;
static ButtonT<MODE_PIN_7> sw_mode_7;
static ButtonT<MODE_PIN_8> sw_mode_8;
static ButtonT<SUB_INT_PIN> sw_sub_int;
static ButtonT<SUB_MAN_PIN> sw_sub_man;
static ButtonT<SUB_EXT_PIN> sw_sub_ext;
static ButtonT<HAND_AXIS_Z_PIN> sw_axis_z;
static ButtonT<HAND_AXIS_X_PIN> sw_axis_x;

static const char *mode_names[] = {
    "AFeed", "SFeed", "Thread", "Chamfer", "Cone", "Sphere", "Divider", "Grind"
};

static void set_sw_level(VirtButton &btn) {
    btn.setBtnLevel(LOW);
}

static void ui_buzzer_mode_beep(void) {
    hal_buzzer_beep_ms(40);
}

/* Галетник держит контакт постоянно — digitalRead, не EncButton.pressing()
 * (debounce EncButton даёт ложный scan==0 на boot и до ~50 мс в poll). */
static uint8_t scan_mode_pin(void) {
    uint8_t raw = 0;
    if (digitalRead(MODE_PIN_1) == LOW) raw = 1;
    else if (digitalRead(MODE_PIN_2) == LOW) raw = 2;
    else if (digitalRead(MODE_PIN_3) == LOW) raw = 3;
    else if (digitalRead(MODE_PIN_4) == LOW) raw = 4;
    else if (digitalRead(MODE_PIN_5) == LOW) raw = 5;
    else if (digitalRead(MODE_PIN_6) == LOW) raw = 6;
    else if (digitalRead(MODE_PIN_7) == LOW) raw = 7;
    else if (digitalRead(MODE_PIN_8) == LOW) raw = 8;
    if (raw == 0) {
        return 0;
    }
    return board_remap_mode_pin(raw);
}

static uint8_t read_mode(void) {
    uint8_t scanned = scan_mode_pin();
    if (scanned > 0) {
        latched_mode = scanned;
    }
    return latched_mode;
}

static uint8_t submode_from_pin(uint8_t pin) {
    if (pin == SUB_INT_PIN) return SUB_INT;
    if (pin == SUB_EXT_PIN) return SUB_EXT;
    return SUB_MANUAL;
}

static uint8_t read_submode(void) {
    if (sw_sub_int.pressing()) latched_submode = SUB_INT;
    else if (sw_sub_man.pressing()) latched_submode = SUB_MANUAL;
    else if (sw_sub_ext.pressing()) latched_submode = SUB_EXT;
    else latched_submode = submode_from_pin(SUB_OFF_PIN);
    return latched_submode;
}

static uint8_t read_mpg_axis(void) {
    if (sw_axis_z.pressing() && !sw_axis_x.pressing()) {
        latched_mpg_axis = AXIS_Z;
    } else if (sw_axis_x.pressing() && !sw_axis_z.pressing()) {
        latched_mpg_axis = AXIS_X;
    }
    return latched_mpg_axis;
}

static uint8_t read_mpg_scale(void) {
    uint8_t v = HAND_SCALE_PORT_RD();
    if (v == 0x02) {
        latched_mpg_scale = 0;  /* D14 LOW — x1step */
    } else if (v == 0x01) {
        latched_mpg_scale = 1;  /* D15 LOW — 0.01 мм */
    }
    return latched_mpg_scale;
}

/* MODE_OFF только после устойчивого scan==0 (антидребезг между позициями 1–8).
 * Выход из OFF — сразу при любом валидном пине. */
static uint8_t mode_off_debounced(uint8_t scanned)
{
    unsigned long now;

    if (scanned > 0U) {
        mode_off_raw_ms = 0UL;
        return 0U;
    }
    now = millis();
    if (mode_off_raw_ms == 0UL) {
        mode_off_raw_ms = (now != 0UL) ? now : 1UL;
        return 0U;
    }
    if ((now - mode_off_raw_ms) >= MODE_OFF_DEBOUNCE_MS) {
        return 1U;
    }
    return 0U;
}

static void sw_tick_all(void) {
    sw_mode_1.tick();
    sw_mode_2.tick();
    sw_mode_3.tick();
    sw_mode_4.tick();
    sw_mode_5.tick();
    sw_mode_6.tick();
    sw_mode_7.tick();
    sw_mode_8.tick();
    sw_sub_int.tick();
    sw_sub_man.tick();
    sw_sub_ext.tick();
    sw_axis_z.tick();
    sw_axis_x.tick();
}

void ui_switches_init(void) {
    set_sw_level(sw_mode_1);
    set_sw_level(sw_mode_2);
    set_sw_level(sw_mode_3);
    set_sw_level(sw_mode_4);
    set_sw_level(sw_mode_5);
    set_sw_level(sw_mode_6);
    set_sw_level(sw_mode_7);
    set_sw_level(sw_mode_8);
    set_sw_level(sw_sub_int);
    set_sw_level(sw_sub_man);
    set_sw_level(sw_sub_ext);
    set_sw_level(sw_axis_z);
    set_sw_level(sw_axis_x);

    sw_tick_all();

    uint8_t scanned = scan_mode_pin();
    latched_mode = (scanned > 0) ? scanned : 1;
    read_mpg_axis();
    read_mpg_scale();

    /* Boot: mode_off=0, таймер debounce не стартуем.
     * Иначе ложный scan==0 (EncButton) + длинный setup ≥ MODE_OFF_DEBOUNCE_MS
     * дают OFF уже на первом poll. Таймер — только в mode_off_debounced(). */
    sw_state.mode_off = 0U;
    mode_off_raw_ms = 0UL;
    sw_state.mode = read_mode();
    sw_state.submode = read_submode();
    sw_state.mpg_axis = latched_mpg_axis;
    sw_state.mpg_scale = latched_mpg_scale;
    last_mode = sw_state.mode;
    last_mode_off = sw_state.mode_off;
    last_submode = sw_state.submode;
    last_mpg_axis = sw_state.mpg_axis;
    last_mpg_scale = sw_state.mpg_scale;
}

void ui_switches_poll(void) {
    sw_tick_all();

    uint8_t scanned = scan_mode_pin();
    uint8_t mode_off = mode_off_debounced(scanned);
    uint8_t mode = read_mode();
    uint8_t sub = read_submode();

    if (mode_off != last_mode_off) {
        if (mode_off) {
            DBG_INFO("UI", "MODE", "OFF");
            motion_jog_resume();  /* стоп joy/MPG/go_lim до EN_X off */
            motor_en_x_release();
            ui_buzzer_mode_beep();
        } else {
            DBG_INFO_VAL("UI", "MODE", ui_switches_get_mode_name(mode), mode);
            ui_buzzer_mode_beep();
        }
        last_mode_off = mode_off;
        last_mode = mode;
    } else if (!mode_off && mode != last_mode) {
        DBG_INFO_VAL("UI", "MODE", ui_switches_get_mode_name(mode), mode);
        ui_buzzer_mode_beep();
        last_mode = mode;
    }
    if (sub != last_submode) {
        const char *sub_name = "Man";
        if (sub == SUB_EXT) sub_name = "Ext";
        else if (sub == SUB_INT) sub_name = "Int";
        DBG_INFO("UI", "SUB", sub_name);
        last_submode = sub;
    }

    sw_state.mode = mode;
    sw_state.mode_off = mode_off;
    sw_state.submode = sub;
    sw_state.mpg_axis = read_mpg_axis();
    sw_state.mpg_scale = read_mpg_scale();

    if (sw_state.mpg_axis != last_mpg_axis) {
        motion_jog_on_axis_select(sw_state.mpg_axis);
        DBG_INFO_VAL("UI", "AXIS", (sw_state.mpg_axis == AXIS_X) ? "X" : "Z",
                     HAND_AXIS_PORT_RD());
        last_mpg_axis = sw_state.mpg_axis;
    }
    if (sw_state.mpg_scale != last_mpg_scale) {
        const char *scale_name = sw_state.mpg_scale ? "0.01mm" : "x1step";
        DBG_INFO_VAL("UI", "SCALE", scale_name, HAND_SCALE_PORT_RD());
        last_mpg_scale = sw_state.mpg_scale;
    }
}

SwitchState_t ui_switches_get_state(void) {
    return sw_state;
}

uint8_t ui_switches_mode_off(void) {
    return sw_state.mode_off;
}

const char *ui_switches_get_mode_name(uint8_t mode) {
    if (mode < 1 || mode > 8) return "?";
    return mode_names[mode - 1];
}
