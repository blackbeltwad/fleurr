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
#include <stddef.h>
#include <stdint.h>

#define MAX_MPU_REGIONS 16
#include "fleurr/task.h"
#include "port.h"
#include "task_internal.h"
#include <stddef.h>
#include <stdint.h>

void port_mpu_configuration(task_handle_t this_task, size_t capacity_bytes) {
  // get power of 2 region size N
  uint8_t N = 32 - __builtin_clz(capacity_bytes - 1);
  if (N < 5) {
    N = 5;
  }

  // Align base address to region size
  uint32_t base_address = (uint32_t)&(this_task->stack[0]) & ~((1UL << N) - 1);

  uint8_t region_index = global_mpu_value % MAX_MPU_REGIONS;
  uint32_t rbar = base_address | (1UL << 4) | region_index;
  global_mpu_value++;

  // Compute subregion disable mask for regions >= 256 bytes
  uint8_t srd = 0;
  if (N >= 8) {
    uint32_t subregion_size = 1UL << (N - 3);
    uint32_t active_subregions =
        (capacity_bytes + subregion_size - 1) / subregion_size;

    if (active_subregions > 8) {
      active_subregions = 8;
    } else if (active_subregions == 0) {
      active_subregions = 1;
    }

    srd = 0xFF << active_subregions;
  }

  // Build RASR attribute value, Execute Never, RW unpriv & priv
  uint32_t rasr = (1UL << 28) |     // XN
                  (0b011UL << 24) | // AP
                  (0b001UL << 19) | // TEX
                  ((uint32_t)srd << 8) | ((N - 1) << 1) | 1UL;

  this_task->RBAR = rbar;
  this_task->RASR = rasr;
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
