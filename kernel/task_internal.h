#ifndef FLEURR_TASK_INTERNAL_H
#define FLEURR_TASK_INTERNAL_H

// Private — only included by kernel/*.c and arch/*/port code. Never
// shipped alongside include/fleurr/*.h.

#include "fleurr/config.h"
#include "fleurr/task.h"
#include <stdint.h>

typedef enum {
  TASK_READY,
  TASK_RUNNING,
  TASK_BLOCKED,
  TASK_SLEEPING,
} task_state_t;

struct task {
  uint8_t stack[MAX_SIZE];
  uint8_t *stack_pointer;
  void *task_arg;
  struct task *next;
  struct task *prev;
  uint32_t sleep_remaining;
  uint8_t priority;      // effective, scheduler-visible priority
  uint8_t base_priority; // real assigned priority, restored after a boost
  task_state_t state;
  uint8_t owns_mutex;
  // TODO: generalize to a held-mutex list for
  // multi-mutex-per-task support (Cortex-M7 era)
};

#endif // FLEURR_TASK_INTERNAL_H
