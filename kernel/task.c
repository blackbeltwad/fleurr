#include "fleurr/task.h"
#include "fleurr/config.h"
#include "fleurr/status.h"
#include "port.h"
#include "scheduler_internal.h"
#include "task_internal.h"
#include <stddef.h>
#include <stdint.h>

// TODO: move existing task_create / task_yield / task_block / task_unblock /
// task_sleep / set_priority / get_current_task bodies here, adapted to:
//   - return fleurr_status_t instead of void where they can fail
//   - support both dynamic (task_create) and static (task_create_static)
//     allocation paths, per docs/ARCHITECTURE.md

fleurr_status_t task_create_static(task_handle_t *out, uint8_t *buffer,
                                   size_t capacity, void (*entry)(void *),
                                   uint8_t priority, void *arg,
                                   task_static_t *storage) {

  task_t *this_task = (task_t *)(storage);
  *out = this_task;
  this_task->stack = buffer;
  this_task->stack_pointer = &this_task->stack[capacity - 1];
  this_task->stack[0] = 0xFF;

  this_task->priority = priority;
  this_task->base_priority = priority;
  this_task->state = TASK_READY;
  this_task->task_arg = arg;
  this_task->blocked_on = NULL;

  port_init_stack_frame(&this_task->stack_pointer, entry, arg);
  append_ready_task(this_task);

  return FLEURR_OK;
}
void task_yield() { port_force_context_switch(); }

void task_block(task_handle_t task) {
  remove_ready_task(task);
  task->state = TASK_BLOCKED;
}

void task_unblock(task_handle_t task) {
  task->state = TASK_READY;
  append_ready_task(task);
}

void task_sleep(uint32_t time_ms) {
  uint8_t old_state = port_enter_critical();
  task_handle_t this_task = get_current_task();
  this_task->state = TASK_SLEEPING;
  this_task->sleep_remaining = time_ms;
  sleep_list_append(this_task);
  port_exit_critical(old_state);
  port_force_context_switch();
}

void set_priority(task_handle_t task, uint8_t priority) {
  uint8_t old_state = port_enter_critical();
  task->priority = priority;
  port_exit_critical(old_state);
}

uint8_t fleurr_enter_critical() { return port_enter_critical(); }
void fleurr_exit_critical(uint8_t old_state) { port_exit_critical(old_state); }
