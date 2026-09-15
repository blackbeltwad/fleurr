#include <stdint.h>
#define MPU_TYPE (*(volatile uint32_t *)0xE000ED90)
#define MPU_CTRL (*(volatile uint32_t *)0xE000ED94)
#define MPU_RNR (*(volatile uint32_t *)0xE000ED98)
#define MPU_RBAR (*(volatile uint32_t *)0xE000ED9C)
#define MPU_RASR (*(volatile uint32_t *)0xE000EDA0)

#define MPU_RBAR_A1 (*(volatile uint32_t *)0xE000EDA4)
#define MPU_RASR_A1 (*(volatile uint32_t *)0xE000EDA8)
#define MPU_RBAR_A2 (*(volatile uint32_t *)0xE000EDAC)
#define MPU_RASR_A2 (*(volatile uint32_t *)0xE000EDB0)
#define MPU_RBAR_A3 (*(volatile uint32_t *)0xE000EDB4)
#define MPU_RASR_A3 (*(volatile uint32_t *)0xE000EDB8)
#define REGION_SIZE_2MB_N 21
#define REGION_SIZE_128KB_N 17
extern int main(void);
extern uint32_t _estack;
void Default_Handler();

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;
extern uint32_t _estack;
extern uint32_t _flash_start;
extern uint32_t _eorigin;
void Reset_Handler(void) {
  /* Define the MPU */
  MPU_RNR = 0; /* Configure .text */
  MPU_RBAR = _flash_start & ~((1UL << REGION_SIZE_2MB_N) - 1);
  MPU_RASR =
      (0UL << 28)       // XN = 0 (Execution Allowed)
      | (0b110UL << 24) // AP = 110 (Read-Only for Privileged AND Unprivileged)
      | (0b000UL << 19) // TEX = 000 (Normal Memory)
      | (1UL << 17)     // C = 1 (Cacheable)
      | (0UL << 16)     // B = 0 (Write-Through, No Write-Allocate)
      | (0UL << 18)     // S = 0 (Not Shareable)
      | (0x00 << 8)     // SRD = 0 (All subregions enabled)
      | (20UL << 1)     // SIZE = 20 (2^(20+1) = 2 MB)
      | (1UL << 0);     // ENABLE = 1

  MPU_RBAR = ((uint32_t)&_eorigin & ~((1UL << REGION_SIZE_128KB_N) - 1)) |
             (1UL << 4) | 1;
  MPU_RASR =
      (1UL << 28)       // XN = 1 (Execute-Never)
      | (0b011UL << 24) // AP = 011 (Read-Write for Privileged and Unprivileged)
      | (0b000UL << 19) // TEX = 000
      | (0UL << 17)     // C = 0 (DTCM is non-cacheable by design on Cortex-M7)
      | (0UL << 16)     // B = 0
      | (16UL << 1)     // SIZE = 16 (128 KB)
      | (1UL << 0);     // ENABLE = 1
  // Copy .data section from Flash to RAM
  uint32_t *src = &_sidata;
  uint32_t *dst = &_sdata;
  while (dst < &_edata) {
    *dst++ = *src++;
  }

  // Zero initialize .bss section in RAM
  dst = &_sbss;
  while (dst < &_ebss) {
    *dst++ = 0;
  }

  //  Set PSP and CONTROL register, then launch main
  __asm volatile("msr PSP, %0 \n"
                 "mrs r0, CONTROL \n"
                 "orr r0, r0, #2 \n"
                 "msr CONTROL, r0 \n"
                 "isb \n" ::"r"(0x2001FFA0)
                 : "r0");
  MPU_CTRL |= 0x05;
  main();

  while (1)
    ;
}
// Weak Exception Prototypes (Internal to startup.c)
void Reset_Handler(void);
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

typedef void (*const ExecFuncPtr)(void);

void Default_Handler() {
  while (1)
    ;
}
//  Vector Table
__attribute__((section(".isr_vector"))) ExecFuncPtr vector_table[] = {
    (ExecFuncPtr)&_estack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // Reserved
    SVC_Handler,
    0,              // Debug Monitor
    0,              // Reserved
    PendSV_Handler, // Points to Default_Handler until port.c defines
                    // PendSV_Handler
    SysTick_Handler,
};
