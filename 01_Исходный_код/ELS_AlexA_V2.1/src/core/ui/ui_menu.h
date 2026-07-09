#ifndef UI_MENU_H
#define UI_MENU_H

#include "../../els_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_menu_init(void);
void ui_menu_poll(void);
uint8_t ui_menu_is_active(void);
void ui_menu_lcd(void);

#ifdef __cplusplus
}
#endif

#endif
