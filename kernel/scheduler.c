#include "fleurr/scheduler.h"
#include "fleurr/task.h"
#include "port.h"
#include "scheduler_internal.h"
#include "task_internal.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void update_sleep_timer(void);
uint32_t CLZ(uint32_t bitmap);
void append_ready_task(task_handle_t this_task);
void choose_ready_task(void);
void sleep_list_append(task_handle_t this_task);
void sleep_list_remove(task_handle_t this_task);
void ready_list_remove(task_handle_t this_task);
void remove_ready_task(task_handle_t this_task);

static struct scheduler scheduler = {.heads = {0},
                                     .tails = {0},
                                     .current_task = NULL,
                                     .ready_bitmap = 0,
                                     .sleep_head = NULL,
                                     .sleep_tail = NULL,
                                     .tick_period_ms = 0};

void *store_and_pop_stack_pointer(void *stack_address) {
  scheduler.current_task->stack_pointer = stack_address;
  append_ready_task(scheduler.current_task);
  choose_ready_task();
  return (void *)(scheduler.current_task->stack_pointer);
}

void scheduler_start(uint32_t time_ms) {
  scheduler.tick_period_ms = time_ms;
  choose_ready_task();
  port_timer_init(time_ms);
  port_start_first_task();
  while (1) {
  };
}

void sleep_list_append(task_handle_t this_task) {
  this_task->next = NULL;
  this_task->prev = scheduler.sleep_tail;
  if (scheduler.sleep_tail != NULL) {
    scheduler.sleep_tail->next = this_task;
  } else {
    scheduler.sleep_head = this_task;
  }
  scheduler.sleep_tail = this_task;
}

void sleep_list_remove(task_handle_t this_task) {
  if (this_task->prev != NULL) {
    this_task->prev->next = this_task->next;
  } else {
    scheduler.sleep_head = this_task->next;
  }
  if (this_task->next != NULL) {
    this_task->next->prev = this_task->prev;
  } else {
    scheduler.sleep_tail = this_task->prev;
  }
  this_task->next = NULL;
  this_task->prev = NULL;
}

void update_sleep_timer() {
  task_handle_t this_task = scheduler.sleep_head;
  while (this_task != NULL) {
    task_handle_t next_task = this_task->next;

    if (this_task->sleep_remaining - scheduler.tick_period_ms <= 0) {
      this_task->sleep_remaining = 0;
      this_task->state = TASK_READY;
      sleep_list_remove(this_task);
      append_ready_task(this_task);
    } else {
      this_task->sleep_remaining -= scheduler.tick_period_ms;
    }

    this_task = next_task;
  }
}
task_handle_t get_current_task() { return scheduler.current_task; }

void append_ready_task(task_handle_t this_task) {
  uint32_t valid_bucket = this_task->priority;
  scheduler.ready_bitmap |= (1UL << valid_bucket);
  this_task->next = NULL;
  this_task->state = TASK_READY;

  if (scheduler.heads[valid_bucket] == NULL) {
    scheduler.heads[valid_bucket] = this_task;
    scheduler.tails[valid_bucket] = this_task;
    this_task->prev = NULL;
  } else {
    task_handle_t old_tail = scheduler.tails[valid_bucket];
    old_tail->next = this_task;
    this_task->prev = old_tail;
    scheduler.tails[valid_bucket] = this_task;
  }
}

void remove_ready_task(task_handle_t this_task) {
  uint32_t bucket = this_task->priority;

  if (this_task->prev != NULL) {
    this_task->prev->next = this_task->next;
  } else {
    scheduler.heads[bucket] = this_task->next;
  }
  if (this_task->next != NULL) {
    this_task->next->prev = this_task->prev;
  } else {
    scheduler.tails[bucket] = this_task->prev;
  }

  if (scheduler.heads[bucket] == NULL) {
    scheduler.ready_bitmap &= ~(1UL << bucket);
  }

  this_task->next = NULL;
  this_task->prev = NULL;
}

void choose_ready_task(void) {
  uint32_t msb_bucket = CLZ(scheduler.ready_bitmap);
  task_handle_t chosen = scheduler.heads[msb_bucket];
  task_handle_t new_head = chosen->next;

  scheduler.current_task = chosen;
  scheduler.current_task->next = NULL;
  scheduler.current_task->prev = NULL;
  scheduler.current_task->state = TASK_RUNNING;

  if (scheduler.tails[msb_bucket] == chosen) {
    scheduler.heads[msb_bucket] = NULL;
    scheduler.tails[msb_bucket] = NULL;
    scheduler.ready_bitmap &= ~(1UL << msb_bucket);
  } else {
    scheduler.heads[msb_bucket] = new_head;
    new_head->prev = NULL;
  }
}

// CLZ is probably whats going to break on different architectures
uint32_t CLZ(uint32_t bitmap) {
  if (bitmap == 0) {
    return 32;
  }
  return 31 - __builtin_clzl(bitmap);
}
