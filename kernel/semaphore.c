#include "fleurr/status.h"
#include "fleurr/sync.h"
#include "fleurr/task.h"
#include "port.h"
#include "scheduler_internal.h"
#include "sync_internal.h"
#include "task_internal.h"
#include <stdint.h>
#include <string.h>

fleurr_status_t sem_create_static(sem_handle_t *out, uint8_t initial_count,
                                  sem_static_t *storage) {
  sem_handle_t this_sem = (sem_handle_t)(storage);
  *out = this_sem;
  this_sem->count = initial_count;
  return FLEURR_OK;
}

fleurr_status_t fleurr_sem_signal(sem_handle_t this_sem) {
  uint8_t old_state = port_enter_critical();
  if (this_sem->wait_head != NULL) {
    struct task *current = this_sem->wait_head;
    this_sem->wait_head = current->next;
    if (this_sem->wait_head != NULL) {
      this_sem->wait_head->prev = NULL;
    } else {
      this_sem->wait_tail = NULL;
    }
    current->next = NULL;
    current->prev = NULL;
    append_ready_task(current);
  } else {
    this_sem->count += 1;
  }

  port_exit_critical(old_state);
  port_force_context_switch();
  return FLEURR_OK;
}

fleurr_status_t fleurr_sem_wait(sem_handle_t this_sem) {
  uint8_t old_state = port_enter_critical();
  struct task *this_task = get_current_task();

  if (this_sem->count > 0) {
    this_sem->count--;
    port_exit_critical(old_state);
    return FLEURR_OK;
  } else {
    this_task->state = TASK_BLOCKED;
    remove_ready_task(this_task);

    struct task *iter = this_sem->wait_head;
    struct task *prev = NULL;

    // Slot it at the at Highest -> Lowest Priority
    while (iter != NULL && this_task->priority <= iter->priority) {
      prev = iter;
      iter = iter->next;
    }

    this_task->prev = prev;
    this_task->next = iter;

    if (iter == NULL) {
      this_sem->wait_tail = this_task;
    } else {
      iter->prev = this_task;
    }
    if (prev == NULL) {
      this_sem->wait_head = this_task;
    } else {
      prev->next = this_task;
    }
    port_exit_critical(old_state);
    port_force_context_switch();
    return FLEURR_OK;
  }
}
