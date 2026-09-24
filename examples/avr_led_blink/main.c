#include "fleurr/fleurr.h"
#include <avr/io.h>
#include <stddef.h>

void red_led(void *arg);
void white_led(void *arg);
void blue_led(void *arg);
void set_only_pin(uint8_t exception);

// Static storage — caller-owned memory, per the static allocation design.
static task_static_t task_a_storage;
static task_static_t task_b_storage;
static task_static_t task_c_storage;

int main(void) {
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
}

void red_led(void *arg) {
  while (1) {
    set_only_pin(7);
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
  DDRD = (1 << pin);
  PORTD = (1 << pin);
}
