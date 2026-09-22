#include "fleurr/task.h"
#include "port.h"
#include "task_internal.h"
#include <stdint.h>
#define SYST_CSR (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018)
#define SYST_CALIB (*(volatile uint32_t *)0xE000E01C)

void port_start_first_task() {
  port_apply_active_task_region(get_current_task());
  uint8_t *stack_pointer = get_current_task()->stack_pointer;

  stack_pointer += INIT_POP;

  uint32_t function_address = *(uint32_t *)stack_pointer;
  void (*start_function)(void *) = (void *)function_address;

  SYST_CSR |= (1 << 0);
  start_function(get_current_task()->task_arg);
}
