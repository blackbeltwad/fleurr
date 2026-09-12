#include "port.h"
#include "scheduler_internal.h"
#include <stdint.h>
#define SYST_CSR (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018)
#define SYST_CALIB (*(volatile uint32_t *)0xE000E01C)
#define HSI_DEFAULT 16000000
#define ICSR (*(volatile uint32_t *)0xE000ED04)
#define SHPR3 (*(volatile uint32_t *)0xE000ED20)

void port_timer_init(uint32_t interval_ms) {
  // Use HSI
  SYST_CSR |= (1 << 2);
  // It will throw an exception at zero
  SYST_CSR |= (1 << 1);
  uint32_t ticks = (interval_ms * (HSI_DEFAULT / 1000UL)) - 1;
  SYST_RVR = ticks & 0x00FFFFFF;
  // Configure PENDSV to lowest priority 255
  SHPR3 |= (0xFFUL << 16);
}

void SysTick_Handler(void) {
  ICSR |= (1UL << 28); // Set PENSV pending bit
  update_sleep_timer();
}
