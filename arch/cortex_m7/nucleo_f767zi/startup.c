#include <stdint.h>
#define MPU_TYPE (*(volatile uint32_t *)0xE000ED90)
#define MPU_CTRL (*(volatile uint32_t *)0xE000ED94)
#define MPU_RNR (*(volatile uint32_t *)0xE000ED98)
#define MPU_RBAR (*(volatile uint32_t *)0xE000ED9C)
#define MPU_RASR (*(volatile uint32_t *)0xE000EDA0)

#define REGION_SIZE_2MB_N 21
#define REGION_SIZE_128KB_N 17
#define REGION_SIZE_512MB_N 28
#define REGION_SIZE_8KB_N 12
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
extern uint32_t _porigin;
extern uint32_t _pxorigin;
void Reset_Handler(void) {
  /* Define the MPU */
  /* Region 0: Flash (.text) - Normal, Cacheable WT */
  MPU_RNR = 0;
  MPU_RBAR = (uint32_t)&_flash_start & ~((1UL << REGION_SIZE_2MB_N) - 1);
  MPU_RASR = (0UL << 28) |     // XN = 0 (Executable)
             (0b110UL << 24) | // AP = RO Priv / RO Unpriv
             (0b000UL << 19) | // TEX = 000
             (1UL << 17) |     // C = 1
             (1UL << 16) |     // B = 1 (WT Cacheable)
             ((REGION_SIZE_2MB_N - 1) << 1) | 1UL;

  /* Region 1: DTCM RAM - Normal, Non-Cacheable */
  MPU_RBAR = ((uint32_t)&_eorigin & ~((1UL << REGION_SIZE_128KB_N) - 1)) |
             (1UL << 4) | 1;
  MPU_RASR = (1UL << 28) |     // XN = 1 (Execute Never)
             (0b011UL << 24) | // AP = RW Priv / RW Unpriv
             (0b001UL << 19) | // TEX = 001 (Normal Non-Cacheable)
             (0UL << 17) |     // C = 0
             (0UL << 16) |     // B = 0
             ((REGION_SIZE_128KB_N - 1) << 1) | 1UL;

  /* Region 2: Peripherals / IO - Shared Device */
  MPU_RBAR = ((uint32_t)&_porigin & ~((1UL << REGION_SIZE_512MB_N) - 1)) |
             (1UL << 4) | 2;
  MPU_RASR = (1UL << 28) |     // XN = 1
             (0b011UL << 24) | // AP = RW Priv / RW Unpriv
             (0b000UL << 19) | // TEX = 000
             (1UL << 18) |     // S = 1 (Shareable)
             (0UL << 17) |     // C = 0
             (1UL << 16) |     // B = 1 (Device)
             ((REGION_SIZE_512MB_N - 1) << 1) | 1UL;

  /* Region 3: FMC & QUADSPI Control - Shared Device */
  MPU_RBAR = ((uint32_t)&_pxorigin & ~((1UL << REGION_SIZE_8KB_N) - 1)) |
             (1UL << 4) | 3;
  MPU_RASR = (1UL << 28) |     // XN = 1
             (0b011UL << 24) | // AP = RW Priv / RW Unpriv
             (0b000UL << 19) | // TEX = 000
             (1UL << 18) |     // S = 1
             (0UL << 17) |     // C = 0
             (1UL << 16) |     // B = 1
             ((REGION_SIZE_8KB_N - 1) << 1) | 1UL;
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
