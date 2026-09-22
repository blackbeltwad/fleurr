#include "fleurr/status.h"
#include "port.h"
#include "task_internal.h"
#include <stdint.h>

#define MPU_RNR (*(volatile uint32_t *)0xE000ED98)
#define MPU_RBAR (*(volatile uint32_t *)0xE000ED9C)
#define MPU_RASR (*(volatile uint32_t *)0xE000EDA0)

#define ACTIVE_TASK_REGION_NUM 5

static uint8_t log2_pow2(size_t capacity) {
  if (capacity < 32 || (capacity & (capacity - 1)) != 0) {
    return 0;
  }
  uint8_t n = 0;
  while ((capacity >> n) > 1) {
    n++;
  }
  return n;
}

fleurr_status_t port_mpu_configuration(task_handle_t this_task,
                                       size_t capacity) {
  uint8_t n = log2_pow2(capacity);

  // We need to do allignment stuff here
  if (n == 0) {
    return FLEURR_ERR_INVALID_ARG;
  }

  uint32_t base = (uint32_t)this_task->stack;

  this_task->RBAR = base & ~((1UL << n) - 1);
  this_task->RASR = (1UL << 28) |     /* XN=1: never executable */
                    (0b011UL << 24) | /* AP=011: RW priv, RW unpriv */
                    (0b001UL << 19) | /* normal memory, shareable */
                    (0UL << 17) | (0UL << 16) | ((n - 1) << 1) |
                    1UL; /* ENABLE */

  return FLEURR_OK;
}

void port_apply_active_task_region(task_handle_t this_task) {
  MPU_RNR = ACTIVE_TASK_REGION_NUM;
  MPU_RBAR = this_task->RBAR;
  MPU_RASR = this_task->RASR;
}
