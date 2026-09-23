#include "port.h"
#include <stdint.h>

#define MPU_TYPE (*(volatile uint32_t *)0xE000ED90)
#define MPU_CTRL (*(volatile uint32_t *)0xE000ED94)
#define MPU_RNR (*(volatile uint32_t *)0xE000ED98)
#define MPU_RBAR (*(volatile uint32_t *)0xE000ED9C)
#define MPU_RASR (*(volatile uint32_t *)0xE000EDA0)

#define REGION_SIZE_2MB_N 21
#define REGION_SIZE_512KB_N 19
#define REGION_SIZE_128KB_N 17
#define REGION_SIZE_512MB_N 28
#define REGION_SIZE_8KB_N 12

/* SRAM1's 512KB-aligned MPU window rounds down to 0x20000000 and would
 * otherwise cover DTCM. Disable the first two 64KB subregions (128KB =
 * exactly DTCM's span) so DTCM stays governed only by region 1. */
#define SRAM1_SRD_DISABLE_DTCM_OVERLAP (0x03UL << 8)

extern int main(void);
extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern uint32_t _flash_start;
extern uint32_t _dtcm_start;
extern uint32_t _sram1_start;
extern uint32_t _porigin;
extern uint32_t _pxorigin;
extern uint32_t _sdtcm_bss;
extern uint32_t _edtcm_bss;
void raise_privilege(void);
void drop_privilege(void);

void Reset_Handler(void) {
  // Region 0: Flash RO Priv / RO Unpriv, executable (XN=0)
  MPU_RNR = 0;
  MPU_RBAR = ((uint32_t)&_flash_start & ~((1UL << REGION_SIZE_2MB_N) - 1));
  MPU_RASR = (0UL << 28) | (0b110UL << 24) | (0b000UL << 19) | (1UL << 17) |
             (1UL << 16) | ((REGION_SIZE_2MB_N - 1) << 1) | 1UL;

  // Region 1: DTCM Priv RW, Unpriv NO ACCESS. Background for all kernel
  // state (TCBs, stacks, mutexes/queues/semaphores). Non-executable.
  MPU_RNR = 1;
  MPU_RBAR = ((uint32_t)&_dtcm_start & ~((1UL << REGION_SIZE_128KB_N) - 1));
  MPU_RASR = (1UL << 28) | (0b001UL << 24) | (0b001UL << 19) | (0UL << 17) |
             (0UL << 16) | ((REGION_SIZE_128KB_N - 1) << 1) | 1UL;

  // Region 2: SRAM1 (.data / .bss) RW Priv / RW Unpriv, non-executable.
  //  SRD bits exclude the first 128KB (DTCM) from this region's grant

  MPU_RNR = 2;
  MPU_RBAR = ((uint32_t)&_sram1_start & ~((1UL << REGION_SIZE_512KB_N) - 1));
  MPU_RASR = (1UL << 28) | (0b011UL << 24) | (0b001UL << 19) | (0UL << 17) |
             (0UL << 16) | SRAM1_SRD_DISABLE_DTCM_OVERLAP |
             ((REGION_SIZE_512KB_N - 1) << 1) | 1UL;

  // Region 3: Periphs / MMIO. Priv RW, Unpriv NO ACCESS  SVC is the
  // only kernel boundary, so unprivileged code has no direct MMIO path.
  MPU_RNR = 3;
  MPU_RBAR = ((uint32_t)&_porigin & ~((1UL << REGION_SIZE_512MB_N) - 1));
  MPU_RASR = (1UL << 28) | (0b001UL << 24) | (0b000UL << 19) | (1UL << 18) |
             (0UL << 17) | (1UL << 16) | ((REGION_SIZE_512MB_N - 1) << 1) | 1UL;

  // Region 4: FMC & QUADSPI. Extended Periphs/ MMIO
  MPU_RNR = 4;
  MPU_RBAR = ((uint32_t)&_pxorigin & ~((1UL << REGION_SIZE_8KB_N) - 1));
  MPU_RASR = (1UL << 28) | (0b001UL << 24) | (0b000UL << 19) | (1UL << 18) |
             (0UL << 17) | (1UL << 16) | ((REGION_SIZE_8KB_N - 1) << 1) | 1UL;

  /* Region 5 (active task stack) is intentionally NOT configured here --
   * it's reprogrammed per-switch in PendSV / port_mpu_configuration().
   * Leaving it disabled at boot means no task stack is unpriv-writable
   * until scheduler_start() brings up the first task. */

  MPU_CTRL = 0x01;

  /* relocate .data to SRAM1 */
  uint32_t *src = &_sidata;
  uint32_t *dst = &_sdata;
  while (dst < &_edata) {
    *dst++ = *src++;
  }

  /* zero .bss in SRAM1 */
  dst = &_sbss;
  while (dst < &_ebss) {
    *dst++ = 0;
  }

  /* caught a bug here never zeroed dtcm.bss and did not initalized task->priv
   * cause restore priv to break!*/
  dst = &_sdtcm_bss;
  while (dst < &_edtcm_bss) {
    *dst++ = 0;
  }

  /* switch to unpriv thread mode using PSP */
  __asm volatile("msr PSP, %0 \n\t"
                 "mrs r0, CONTROL \n\t"
                 "orr r0, r0, #2 \n\t"
                 "bic r0, r0, #1 \n\t"
                 "msr CONTROL, r0 \n\t"
                 "isb \n\t" ::"r"((uint32_t)&_sdata + 0x1000)
                 : "r0");

  main();

  while (1)
    ;
}
void __attribute__((naked)) SVC_Handler(void) {
  __asm volatile("mrs r0, CONTROL \n\t"
                 "bic r0, r0, #1  \n\t"
                 "msr CONTROL, r0 \n\t"
                 "isb             \n\t"
                 "bx lr           \n\t");
}

void Default_Handler(void) {
  while (1)
    ;
}

void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));

typedef void (*const ExecFuncPtr)(void);

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
    0,
    SVC_Handler,
    0,
    0,
    PendSV_Handler,
    SysTick_Handler,
};
