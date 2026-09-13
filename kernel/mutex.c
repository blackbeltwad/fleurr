#include "fleurr/status.h"
#include "fleurr/sync.h"
#include "fleurr/task.h"
#include "port.h"
#include "scheduler_internal.h"
#include "sync_internal.h"
#include "task_internal.h"
#include <stddef.h>
#include <stdint.h>

void inheritor_protocol(mutex_handle_t mutex);
static void remove_held_mutex(task_handle_t task, mutex_handle_t mutex);
static uint8_t recompute_priority(task_handle_t task);

fleurr_status_t mutex_create_static(mutex_handle_t *out,
                                    mutex_protocol_t protocol,
                                    uint8_t ceiling_priority,
                                    mutex_static_t *storage) {
  mutex_t *this_mutex = (mutex_t *)storage;
  *out = this_mutex;

  this_mutex->owner = NULL;
  this_mutex->protocol = protocol;
  this_mutex->block_head = NULL;
  this_mutex->block_tail = NULL;
  this_mutex->owner_next = NULL;
  this_mutex->owner_prev = NULL;
  if (protocol == PROTOCOL_CEILING) {
    this_mutex->ceiling_priority = ceiling_priority;
  } else {
    this_mutex->ceiling_priority = 0;
  }

  return FLEURR_OK;
}

fleurr_status_t mutex_lock(mutex_handle_t mutex) {
  uint8_t old_state = port_enter_critical();
  task_handle_t this_task = get_current_task();

  if (mutex->owner == NULL) {
    mutex->owner = this_task;
    mutex->owner_next = this_task->held_mutexes_head;
    mutex->owner_prev = NULL;
    if (this_task->held_mutexes_head != NULL) {
      this_task->held_mutexes_head->owner_prev = mutex;
    }
    this_task->held_mutexes_head = mutex;

    port_exit_critical(old_state);
    return FLEURR_OK;
  }

  if (mutex->owner == this_task) {
    port_exit_critical(old_state);
    return FLEURR_MUTEX_IN_USE;
  }

  // contended: block this task and insert into the wait list, highest priority
  // first
  remove_ready_task(this_task);
  this_task->state = TASK_BLOCKED;
  this_task->blocked_on = mutex;

  task_handle_t iter = mutex->block_head;
  task_handle_t prev = NULL;
  while (iter != NULL && iter->priority >= this_task->priority) {
    prev = iter;
    iter = iter->next;
  }
  this_task->prev = prev;
  this_task->next = iter;
  if (prev != NULL) {
    prev->next = this_task;
  } else {
    mutex->block_head = this_task;
  }
  if (iter != NULL) {
    iter->prev = this_task;
  } else {
    mutex->block_tail = this_task;
  }

  if (mutex->protocol == PROTOCOL_INHERIT) {
    inheritor_protocol(mutex);
  } else if (mutex->protocol == PROTOCOL_CEILING) {
    // NOT IMPLEMENTED YET
  }

  port_exit_critical(old_state);
  port_force_context_switch();
  // this_task resumes here once it becomes owner
  return FLEURR_OK;
}

void inheritor_protocol(mutex_handle_t mutex) {
  task_handle_t waiter = get_current_task();
  task_handle_t owner = mutex->owner;

  while (owner->priority < waiter->priority) {
    if (owner->state == TASK_READY) {
      remove_ready_task(owner);
      owner->priority = waiter->priority;
      append_ready_task(owner);
      break;
    } else if (owner->state == TASK_BLOCKED) {
      owner->priority = waiter->priority;
      owner = owner->blocked_on->owner;
    } else {
      break;
    }
  }
}

fleurr_status_t mutex_unlock(mutex_handle_t mutex) {
  uint8_t old_state = port_enter_critical();
  task_handle_t this_task = get_current_task();

  if (mutex->owner != this_task) {
    port_exit_critical(old_state);
    return FLEURR_MUTEX_NOT_OWNER;
  }

  remove_held_mutex(this_task, mutex);

  uint8_t new_priority = recompute_priority(this_task);
  if (new_priority != this_task->priority) {
    if (this_task->state == TASK_READY) {
      remove_ready_task(this_task);
      this_task->priority = new_priority;
      append_ready_task(this_task);
    } else {
      this_task->priority = new_priority;
    }
  }

  if (mutex->block_head != NULL) {
    task_handle_t next_owner = mutex->block_head;
    mutex->block_head = next_owner->next;
    if (mutex->block_head != NULL) {
      mutex->block_head->prev = NULL;
    } else {
      mutex->block_tail = NULL;
    }
    next_owner->next = NULL;
    next_owner->prev = NULL;
    next_owner->blocked_on = NULL;

    mutex->owner = next_owner;
    mutex->owner_next = next_owner->held_mutexes_head;
    mutex->owner_prev = NULL;
    if (next_owner->held_mutexes_head != NULL) {
      next_owner->held_mutexes_head->owner_prev = mutex;
    }
    next_owner->held_mutexes_head = mutex;

    next_owner->state = TASK_READY;
    append_ready_task(next_owner);
  } else {
    mutex->owner = NULL;
  }

  port_exit_critical(old_state);
  port_force_context_switch();
  return FLEURR_OK;
}

static void remove_held_mutex(task_handle_t task, mutex_handle_t mutex) {
  if (mutex->owner_prev != NULL) {
    mutex->owner_prev->owner_next = mutex->owner_next;
  } else {
    task->held_mutexes_head = mutex->owner_next;
  }
  if (mutex->owner_next != NULL) {
    mutex->owner_next->owner_prev = mutex->owner_prev;
  }
  mutex->owner_next = NULL;
  mutex->owner_prev = NULL;
}

static uint8_t recompute_priority(task_handle_t task) {
  uint8_t max_priority = task->base_priority;
  mutex_handle_t m = task->held_mutexes_head;
  while (m != NULL) {
    if (m->block_head != NULL && m->block_head->priority > max_priority) {
      max_priority = m->block_head->priority;
    }
    m = m->owner_next;
  }
  return max_priority;
}
