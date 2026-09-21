#ifndef SVC_H
#define SVC_H
#include <stdint.h>
void port_restore_priv(void);
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
#endif
