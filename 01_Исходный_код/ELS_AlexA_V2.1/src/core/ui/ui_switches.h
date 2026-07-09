#ifndef UI_SWITCHES_H
#define UI_SWITCHES_H

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t mode;
    uint8_t submode;
    uint8_t mpg_axis;
    uint8_t mpg_scale;
} SwitchState_t;

void ui_switches_init(void);
void ui_switches_poll(void);
SwitchState_t ui_switches_get_state(void);
const char *ui_switches_get_mode_name(uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif
