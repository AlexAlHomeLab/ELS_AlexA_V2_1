#ifndef UI_MENU_H
#define UI_MENU_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


void ui_menu_init(void);
void ui_menu_update(void);
void ui_menu_handle_navigation(void);
void ui_menu_settings(void);
void ui_menu_set_mode(uint8_t mode);
void ui_menu_set_submode(uint8_t submode);


#ifdef __cplusplus
}
#endif
#endif
