#ifndef UI_IO_H
#define UI_IO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool EB_read(uint8_t pin);
void EB_mode(uint8_t pin, uint8_t mode);
uint32_t EB_uptime(void);

#ifdef __cplusplus
}
#endif

#endif
