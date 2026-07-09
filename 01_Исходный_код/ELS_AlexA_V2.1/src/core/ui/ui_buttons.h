#ifndef UI_BUTTONS_H
#define UI_BUTTONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t left;
    uint8_t right;
    uint8_t up;
    uint8_t down;
    uint8_t select;
    uint8_t select_hold;
    uint8_t joy_left;
    uint8_t joy_right;
    uint8_t joy_up;
    uint8_t joy_down;
    uint8_t joy_rapid;
    uint8_t limit_left;
    uint8_t limit_front;
    uint8_t limit_right;
    uint8_t limit_rear;
} ButtonState_t;

typedef struct {
    uint8_t left;
    uint8_t right;
    uint8_t up;
    uint8_t down;
    uint8_t select;
    uint8_t select_hold;
} ButtonClicks_t;

void ui_buttons_init(void);
void ui_buttons_poll(void);
void ui_buttons_reset_joy(void);
ButtonState_t ui_buttons_get_state(void);
ButtonClicks_t ui_buttons_get_clicks(void);
uint8_t ui_buttons_feed_joy_click(void);
uint8_t ui_buttons_feed_joy_on(void);

#ifdef __cplusplus
}
#endif

#endif
