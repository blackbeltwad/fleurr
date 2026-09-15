#include "fleurr/scheduler.h"
#include "fleurr/task.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void red_led(void *arg);
void white_led(void *arg);
void blue_led(void *arg);
void set_only_pin(uint8_t pin);
#define SYST_CSR (*(volatile uint32_t *)0xE000E010)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018)
#define SYST_CALIB (*(volatile uint32_t *)0xE000E01C)

#define RCC_BASE (0x40023800UL)
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30))

#define GPIOD_BASE (0x40020C00UL)
#define GPIOD_MODER (*(volatile uint32_t *)(GPIOD_BASE + 0x00))
#define GPIOD_ODR (*(volatile uint32_t *)(GPIOD_BASE + 0x14))

static task_static_t task_a_storage;
static task_static_t task_b_storage;
static task_static_t task_c_storage;

int main(void) {

  RCC_AHB1ENR |= (1U << 3);

  GPIOD_MODER &= ~((3U << (4 * 2)) | (3U << (5 * 2)) | (3U << (6 * 2)));
  GPIOD_MODER |= ((1U << (4 * 2)) | (1U << (5 * 2)) | (1U << (6 * 2)));

  task_handle_t task_a;
  task_handle_t task_b;
  task_handle_t task_c;

  uint8_t a_buffer[256];
  uint8_t b_buffer[256];
  uint8_t c_buffer[256];

  task_create_static(&task_a, a_buffer, 256, &red_led, 1, NULL,
                     &task_a_storage);
  task_create_static(&task_b, b_buffer, 256, &white_led, 1, NULL,
                     &task_b_storage);
  task_create_static(&task_c, c_buffer, 256, &blue_led, 1, NULL,
                     &task_c_storage);

  scheduler_start(200); // never returns
  while (1) {
  }
}

void red_led(void *arg) {
  while (1) {
    set_only_pin(4);
  }
}

void white_led(void *arg) {
  while (1) {
    set_only_pin(6);
  }
}

void blue_led(void *arg) {
  while (1) {
    set_only_pin(5);
  }
}

void set_only_pin(uint8_t pin) {
  uint32_t mask = (1U << 4) | (1U << 5) | (1U << 6);
  GPIOD_ODR = (GPIOD_ODR & ~mask) | (1U << pin);
}
