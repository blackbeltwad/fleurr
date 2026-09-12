#include "avr/io.h"
#include "port.h"
#include <stdint.h>

void port_exit_critical(uint8_t old_state) { SREG = old_state; }

uint8_t port_enter_critical(void) {
  uint8_t old_state = SREG;
  SREG &= ~(1 << 7);
  return old_state;
}
