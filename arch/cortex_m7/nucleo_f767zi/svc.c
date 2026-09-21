#include "fleurr/task.h"
#include "port.h"

void port_restore_priv(void) {
  struct task *this_task = get_current_task();

  __asm__ volatile("msr control, %0 \n"
                   "isb \n"
                   :
                   : "r"(this_task->priv)
                   : "memory");
}
