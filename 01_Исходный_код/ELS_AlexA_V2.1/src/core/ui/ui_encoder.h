#ifndef UI_ENCODER_H
#define UI_ENCODER_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif


void ui_encoder_init(void);
void ui_encoder_poll(void);
int32_t ui_encoder_get_mpg_pos(void);
int32_t ui_encoder_get_mpg_delta(void);
void ui_encoder_reset_mpg(void);

void hand_mpg_isr_step(void);

#ifdef __cplusplus
}
#endif
#endif
