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
  this_task->priv = 1; // unpriv
  port_init_stack_frame(&this_task->stack_pointer, entry, arg);
  fleurr_status_t mpu_status = port_mpu_configuration(this_task, capacity);
  if (mpu_status != FLEURR_OK) {
    return mpu_status;
  }

  append_ready_task(this_task);

  return FLEURR_OK;
}
void task_yield() {
  fleurr_raise_priv();
  port_force_context_switch();
  fleurr_drop_priv();
}

void task_block(task_handle_t task) {
  fleurr_raise_priv();
  uint8_t old_state = port_enter_critical();
  remove_ready_task(task);
  task->state = TASK_BLOCKED;
  port_exit_critical(old_state);
  fleurr_drop_priv();
}

void task_unblock(task_handle_t task) {
  fleurr_raise_priv();
  uint8_t old_state = port_enter_critical();
  task->state = TASK_READY;
  append_ready_task(task);
  port_exit_critical(old_state);
  fleurr_drop_priv();
}

void task_sleep(uint32_t time_ms) {
  fleurr_raise_priv();
  uint8_t old_state = port_enter_critical();
  task_handle_t this_task = get_current_task();
  this_task->state = TASK_SLEEPING;
  this_task->sleep_remaining = time_ms;
  sleep_list_append(this_task);
  port_exit_critical(old_state);
  port_force_context_switch();
  fleurr_drop_priv();
}

/* NOTE, unrelated to privilege wrapping: if `task` is currently TASK_READY,
 * changing ->priority in place without a remove_ready_task/append_ready_task
 * cycle leaves it in its OLD priority bucket while the field says otherwise
 * -- the bucketed ready queue's bitmap and bucket contents go out of sync
 * with the task's actual priority. fleurr_mutex_unlock's recompute_priority
 * path already does this correctly (see the remove/append dance there).
 * This looks like a real, separate bug -- flagging, not fixing, since it's
 * outside what was asked here. */
void set_priority(task_handle_t task, uint8_t priority) {
  fleurr_raise_priv();
  uint8_t old_state = port_enter_critical();
  task->priority = priority;
  port_exit_critical(old_state);
  fleurr_drop_priv();
}

uint8_t fleurr_enter_critical() {
  fleurr_raise_priv();
  return port_enter_critical();
}

void fleurr_exit_critical(uint8_t old_state) {
  port_exit_critical(old_state);
  fleurr_drop_priv();
}
