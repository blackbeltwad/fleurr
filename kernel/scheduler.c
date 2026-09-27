#include "fleurr/scheduler.h"
#include "fleurr/queue.h"
#include "fleurr/task.h"
#include "port.h"
#include "scheduler_internal.h"
#include "sync_internal.h"
#include "task_internal.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

void update_sleep_timer(void);
uint32_t get_highest_priority_bit(uint32_t bitmap);
void append_ready_task(task_handle_t this_task);
void choose_ready_task(void);
void sleep_list_append(task_handle_t this_task);
void sleep_list_remove(task_handle_t this_task);
void remove_ready_task(task_handle_t this_task);

static struct scheduler scheduler = {.heads = {0},
                                     .tails = {0},
                                     .current_task = NULL,
                                     .ready_bitmap = 0,
                                     .sleep_head = NULL,
                                     .sleep_tail = NULL,
                                     .deadline_head = NULL,
                                     .deadline_tail = NULL,
                                     .tick_period_ms = 0};

void *store_and_pop_stack_pointer(void *stack_address) {
  task_handle_t out = scheduler.current_task;

  if (out != NULL) {
    out->stack_pointer = stack_address;
    if (out->state == TASK_RUNNING) {
      append_ready_task(out);
    }
  }

  choose_ready_task();
  port_context_switch(out, scheduler.current_task);
  return scheduler.current_task->stack_pointer;
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

void update_sleep_timer(void) {
  task_handle_t this_task = scheduler.sleep_head;
  while (this_task != NULL) {
    task_handle_t next_task = this_task->next;

    // Caught an underflow here
    if (this_task->sleep_remaining <= scheduler.tick_period_ms) {
      this_task->sleep_remaining = 0;
      sleep_list_remove(this_task);
      append_ready_task(this_task);
    } else {
      this_task->sleep_remaining -= scheduler.tick_period_ms;
    }

    this_task = next_task;
  }
}

static void generic_list_remove(task_handle_t task, task_handle_t *head,
                                task_handle_t *tail) {
  if (task->prev != NULL) {
    task->prev->next = task->next;
  } else {
    *head = task->next;
  }
  if (task->next != NULL) {
    task->next->prev = task->prev;
  } else {
    *tail = task->prev;
  }
  task->next = NULL;
  task->prev = NULL;
}

static void remove_from_wait_list(task_handle_t task) {
  switch (task->wait_kind) {
  case WAIT_MUTEX: {
    mutex_handle_t m = (mutex_handle_t)task->blocked_on;
    generic_list_remove(task, &m->block_head, &m->block_tail);
    break;
  }
  case WAIT_SEM: {
    sem_handle_t s = (sem_handle_t)task->blocked_on;
    generic_list_remove(task, &s->wait_head, &s->wait_tail);
    break;
  }
  case WAIT_QUEUE_SEND: {
    queue_handle_t q = (queue_handle_t)task->blocked_on;
    generic_list_remove(task, &q->send_wait_head, &q->send_wait_tail);
    break;
  }
  case WAIT_QUEUE_RECEIVE: {
    queue_handle_t q = (queue_handle_t)task->blocked_on;
    generic_list_remove(task, &q->receive_wait_head, &q->receive_wait_tail);
    break;
  }
  case WAIT_NONE:
  default:
    break;
  }
  task->blocked_on = NULL;
  task->wait_kind = WAIT_NONE;
}

void deadline_list_append(task_handle_t this_task, uint32_t timeout_ms) {
  this_task->timeout_remaining = timeout_ms;
  this_task->timeout_next = NULL;
  this_task->timeout_prev = scheduler.deadline_tail;
  if (scheduler.deadline_tail != NULL) {
    scheduler.deadline_tail->timeout_next = this_task;
  } else {
    scheduler.deadline_head = this_task;
  }
  scheduler.deadline_tail = this_task;
}

static void deadline_list_remove(task_handle_t task) {
  if (task->timeout_prev != NULL) {
    task->timeout_prev->timeout_next = task->timeout_next;
  } else {
    scheduler.deadline_head = task->timeout_next;
  }
  if (task->timeout_next != NULL) {
    task->timeout_next->timeout_prev = task->timeout_prev;
  } else {
    scheduler.deadline_tail = task->timeout_prev;
  }
  task->timeout_next = NULL;
  task->timeout_prev = NULL;
}

void cancel_deadline_if_any(task_handle_t this_task) {
  if (this_task->has_deadline) {
    deadline_list_remove(this_task);
    this_task->has_deadline = 0;
  }
}

void update_deadline_timer(void) {
  task_handle_t task = scheduler.deadline_head;
  while (task != NULL) {
    task_handle_t next_task = task->timeout_next;

    if (task->timeout_remaining <= scheduler.tick_period_ms) {
      deadline_list_remove(task);
      task->has_deadline = 0;
      remove_from_wait_list(task);
      task->timed_out = 1;
      append_ready_task(task); /* sets state = TASK_READY internally */
    } else {
      task->timeout_remaining -= scheduler.tick_period_ms;
    }

    task = next_task;
  }
}

task_handle_t get_current_task(void) { return scheduler.current_task; }

void append_ready_task(task_handle_t this_task) {
  uint32_t valid_bucket = this_task->priority;
  scheduler.ready_bitmap |= (1UL << valid_bucket);
  this_task->next = NULL;
  this_task->prev = NULL;
  this_task->state = TASK_READY;

  if (scheduler.heads[valid_bucket] == NULL) {
    scheduler.heads[valid_bucket] = this_task;
    scheduler.tails[valid_bucket] = this_task;
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

  if (scheduler.ready_bitmap == 32) {
    // IDLE TASK;
    return;
  }

  uint32_t msb_bucket = get_highest_priority_bit(scheduler.ready_bitmap);
  task_handle_t chosen = scheduler.heads[msb_bucket];

  remove_ready_task(chosen);

  scheduler.current_task = chosen;
  scheduler.current_task->state = TASK_RUNNING;
}

// CLZ is probably whats going to break on different architectures
uint32_t get_highest_priority_bit(uint32_t bitmap) {
  if (bitmap == 0) {
    return 32;
  }
  return 31 - __builtin_clzl(bitmap);
}
