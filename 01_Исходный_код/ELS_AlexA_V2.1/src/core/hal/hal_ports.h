#ifndef HAL_PORTS_H
#define HAL_PORTS_H






#include "../../els_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* --- Номера портов --- */
#define PORT_B 0
#define PORT_C 1
#define PORT_D 2
#define PORT_E 3
#define PORT_F 4
#define PORT_G 5
#define PORT_H 6
#define PORT_J 7
#define PORT_K 8
#define PORT_L 9

/* --- Быстрые операции с портами --- */











/* --- Быстрые операции с портами --- */
void port_set(uint8_t port, uint8_t pin);
void port_clear(uint8_t port, uint8_t pin);
uint8_t port_read(uint8_t port, uint8_t pin);
void port_toggle(uint8_t port, uint8_t pin);

/* --- Макросы для максимальной скорости --- */






#ifdef __cplusplus
}
#endif
#endif
