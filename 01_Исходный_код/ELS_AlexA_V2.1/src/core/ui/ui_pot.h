#ifndef UI_POT_H
#define UI_POT_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


/* --- Подрежимы --- */




void ui_pot_init(void);
void ui_pot_read(void);
uint16_t ui_pot_get_value(void);
uint8_t ui_pot_get_percent(void);
uint8_t ui_pot_has_changed(void);
uint8_t ui_pot_get_mode(void);
uint8_t ui_pot_get_submode(void);
uint8_t ui_pot_poll(void);


#ifdef __cplusplus
}
#endif
#endif
