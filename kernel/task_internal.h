#ifndef FLEURR_TASK_INTERNAL_H
#define FLEURR_TASK_INTERNAL_H

// Private — only included by kernel/*.c and arch/*/port code. Never
// shipped alongside include/fleurr/*.h.

#include "fleurr/config.h"
#include "fleurr/sync.h"
#include "fleurr/task.h"
#include <stdint.h>

typedef enum {
  TASK_READY,
  TASK_RUNNING,
  TASK_BLOCKED,
  TASK_SLEEPING,
} task_state_t;

typedef enum {
  WAIT_NONE,
  WAIT_MUTEX,
  WAIT_SEM,
  WAIT_QUEUE_SEND,
  WAIT_QUEUE_RECEIVE,
} wait_kind_t;

struct task {
  uint8_t *stack;
  uint8_t *stack_pointer;
  void *task_arg;
  struct task *next;
  struct task *prev;
  struct task *timeout_next; // deadline-list linkage, independent of next/prev
  struct task *timeout_prev;
  uint32_t RBAR;
  uint32_t RASR;
  void *blocked_on;           // mutex_handle_t / sem_handle_t / queue_handle_t
  wait_kind_t wait_kind;      // which kind blocked_on actually is
  uint32_t timeout_remaining; // ms left on this task's current wait deadline
  uint8_t has_deadline;       // 1 if currently linked into the deadline list
  uint8_t timed_out;          // set by expiry, read by the caller on wake
  mutex_handle_t held_mutexes_head;
  uint32_t sleep_remaining; // task_sleep()'s own list, unrelated to the above
  uint8_t priority;         // effective, scheduler-visible priority
  uint8_t base_priority;    // real assigned priority, restored after a boost
  task_state_t state;
  uint8_t priv;
};

#endif // FLEURR_TASK_INTERNAL_H
