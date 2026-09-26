#ifndef FLEURR_SCHEDULER_INTERNAL_H
#define FLEURR_SCHEDULER_INTERNAL_H

#include "fleurr/config.h"
#include "task_internal.h"
#include <stddef.h>

struct scheduler {
  struct task *heads[NUM_OF_PRIORITY];
  struct task *tails[NUM_OF_PRIORITY];
  struct task *current_task;
  uint32_t ready_bitmap;
  task_handle_t sleep_head;
  task_handle_t sleep_tail;
  task_handle_t deadline_head;
  task_handle_t deadline_tail;
  uint32_t tick_period_ms;
};

// Private methods shared across kernel/*.c
void *store_and_pop_stack_pointer(void *stack_address);
void update_sleep_timer(void);
void update_deadline_timer(void);
void deadline_list_append(task_handle_t this_task, uint32_t timeout_ms);
void cancel_deadline_if_any(task_handle_t this_task);
struct task *select_next_task(void);
uint32_t CLZ(uint32_t bitmap);
void append_ready_task(task_handle_t this_task);
void choose_ready_task(void);
void sleep_list_append(task_handle_t this_task);
void sleep_list_remove(task_handle_t this_task);
void ready_list_remove(task_handle_t this_task);
void remove_ready_task(task_handle_t this_task);

#endif // FLEURR_SCHEDULER_INTERNAL_H
