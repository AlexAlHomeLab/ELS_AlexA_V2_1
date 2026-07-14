#ifndef MOTOR_EN_H
#define MOTOR_EN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Soft-latch Enable оси X (EN_X_SOFT_LATCH). EN_Z не обслуживается. */

void motor_en_x_init(void);       /* OUTPUT + disable при soft-latch; иначе no-op */
void motor_en_x_ensure(void);     /* если не latched — EN HIGH, latch, settle */
void motor_en_x_release(void);    /* EN LOW, сброс latch (MODE_OFF) */
uint8_t motor_en_x_is_latched(void);

/* E-Stop → EN_X: пока без реализации
 * void motor_en_x_on_estop(void);
 */

#ifdef __cplusplus
}
#endif

#endif
