#ifndef UI_SWITCHES_H
#define UI_SWITCHES_H

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t mode;       /* защёлкнутый режим 1..8 для FSM/UI имени */
    uint8_t mode_off;   /* 1 — сырой скан 0 (все MODE_PIN неактивны) */
    uint8_t submode;
    uint8_t mpg_axis;
    uint8_t mpg_scale;
} SwitchState_t;

void ui_switches_init(void);
void ui_switches_poll(void);
SwitchState_t ui_switches_get_state(void);
const char *ui_switches_get_mode_name(uint8_t mode);
uint8_t ui_switches_mode_off(void);  /* 1 — MODE_OFF */

#ifdef __cplusplus
}
#endif

#endif
