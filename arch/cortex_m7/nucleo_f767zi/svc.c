#include "svc.h"
#include "fleurr/task.h"
#include "port.h"
#include "scheduler_internal.h"

void fleurr_raise_priv(void) {
  __asm__ volatile("svc #0");
  task_handle_t this_task = get_current_task();
  this_task->priv = 0;
}

void port_restore_priv(void) {
  task_handle_t this_task = get_current_task();
  uint32_t control = 0;
  __asm__ volatile("mrs %0, control" : "=r"(control));
  control = (control & ~0x1UL) | (this_task->priv & 0x1UL);
  __asm__ volatile("msr control, %0 \n\t isb" ::"r"(control));
}

void fleurr_drop_priv(void) {
  task_handle_t this_task = get_current_task();
  this_task->priv = 1;
  __asm__ volatile("mrs r0, control \n\t"
                   "orr r0, r0, #1  \n\t"
                   "msr control, r0 \n\t"
                   "isb             \n\t" ::
                       : "r0");
}
