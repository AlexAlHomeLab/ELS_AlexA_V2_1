#ifndef SERIAL_CONFIG_H
#define SERIAL_CONFIG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void serial_config_init(void);
void serial_config_poll(void);

#ifdef __cplusplus
}
#endif

#endif
