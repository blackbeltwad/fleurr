#include "fleurr/config.h"
#include "port.h"
#include "svc.h"
#include <stdint.h>

uint8_t port_enter_critical() {
  uint8_t old_state = 0;
  __asm__ volatile("mrs %0, basepri \n"
                   "msr basepri, %1 \n"
                   "isb \n"
                   : "=r"(old_state)
                   : "r"(PORT_MAX_INTERRUPT_PRIORITY)
                   : "memory");
  return old_state;
}

void port_exit_critical(uint8_t old_state) {
  __asm__ volatile("msr basepri, %0 \n"
                   "isb \n"
                   :
                   : "r"(old_state)
                   : "memory");
  port_restore_priv();
}
