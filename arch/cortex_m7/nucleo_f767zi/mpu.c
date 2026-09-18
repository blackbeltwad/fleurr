#include "fleurr/task.h"
#include "port.h"
#include "task_internal.h"
#include <math.h>
#include <stdint.h>
#define MPU_RBAR (*(volatile uint32_t *)0xE000ED9C)
#define MPU_RASR (*(volatile uint32_t *)0xE000EDA0)
#define MPU_TYPE (*(volatile uint32_t *)0xE000ED90)
#define MPU_CTRL (*(volatile uint32_t *)0xE000ED94)

// Were gonna default it to No Access
// For the handling between during context switch, i think during the mpu switch
// we have to enter critical If a higher prio hits while inside in the interrupt
// it might break

/* Pseudo Code
  after stack pointers swap
  portentercrit
  disable mpu
  use task fields for the region data for the swap that ill make soon
  enable mpu
  portexitcrit
*/

// This will be inside task_create_static
void port_mpu_configuration(task_handle_t this_task, size_t capacity) {
  uint8_t N = 32 - __builtin_clz(capacity - 1);
  if (N < 5)
    N = 5;

  MPU_RBAR = ((uint32_t)&(this_task->stack[capacity - 1]) & ~((1UL << N) - 1)) |
             (1UL << 4) | 4;

  uint8_t srd = 0;
  if (N >= 8) {
    uint32_t subregion_size = 1UL << (N - 3);
    uint32_t active_subregions =
        (capacity + subregion_size - 1) / subregion_size;
    if (active_subregions < 8) {
      srd = ~((1U << active_subregions) - 1);
    }
  }

  MPU_RASR = (0UL << 28) | (0b000UL << 24) | ((uint32_t)srd << 8) |
             ((N - 1) << 1) | 1UL;

  this_task->RBAR = MPU_RBAR;
  this_task->RASR = MPU_RASR;
}

void port_mpu_swap() {
  uint8_t oldstate = port_enter_critical();
  MPU_CTRL &= ~(1 << 0);
  struct task *this_task = get_current_task();
  MPU_RBAR = this_task->RBAR;
  MPU_RASR = this_task->RASR;
  MPU_CTRL |= (1 << 0);
  port_exit_critical(oldstate);
}
