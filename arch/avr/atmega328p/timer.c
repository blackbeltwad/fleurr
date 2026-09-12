#include "port.h"
#include <avr/common.h>
#include <avr/io.h>
#include <stdint.h>
#define CLOCK_VALUE_MS 0.064

// TODO: move existing timer_init() body here.
void port_timer_init(uint32_t interval_ms) {

  // Set the timer to normal
  TCCR1A &= ~(1 << 1);
  TCCR1A &= ~(1 << 0);
  TCCR1B |= ((1 << 3));
  TCCR1B &= ~(1 << 4);
  // Set the context switch to occur in roughly ~10ms

  TCCR1B |= (1 << 2);
  TCCR1B &= ~(1 << 1);
  TCCR1B |= (1 << 0);
  uint16_t OCR1A_value = interval_ms / CLOCK_VALUE_MS;

  OCR1AH = (OCR1A_value >> 8);
  OCR1AL = (OCR1A_value & 0x00FF);

  SREG &= ~(1 << 7);
  // Set the interrupts on OCR0A match
  TIMSK1 |= (1 << 1);
  TIFR1 |= (1 << 1);
}
