#include "fleurr/queue.h"
#include "fleurr/status.h"
#include "fleurr/task.h"
#include "port.h"
#include "scheduler_internal.h"
#include "string.h"
#include "sync_internal.h"
#include "task_internal.h"
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t n) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;

  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return dest;
}

static void copy_to_buffer(queue_handle_t q, const void *item_ptr) {
  memcpy(&(q->buffer[q->head]), item_ptr, q->item_size);
  q->head = (q->head + q->item_size) % (q->capacity * q->item_size);
  q->count++;
}

static void copy_from_buffer(queue_handle_t q, void *receive_buffer) {
  memcpy(receive_buffer, &(q->buffer[q->tail]), q->item_size);
  q->tail = (q->tail + q->item_size) % (q->capacity * q->item_size);
  q->count--;
}

static void unblock_head_task(task_handle_t *head, task_handle_t *tail) {
  task_handle_t chosen = *head;
  if (chosen != NULL) {
    *head = chosen->next;
    if (*head != NULL) {
      (*head)->prev = NULL;
    } else {
      *tail = NULL;
    }
    chosen->next = NULL;
    chosen->prev = NULL;
    append_ready_task(chosen);
  }
}

fleurr_status_t queue_create_static(queue_handle_t *out, size_t item_size,
                                    size_t capacity, queue_static_t *storage,
                                    uint8_t *buffer) {
  queue_handle_t this_queue = (queue_handle_t)(storage);
  *out = this_queue;

  this_queue->buffer = buffer;
  this_queue->capacity = capacity;
  this_queue->item_size = item_size;

  this_queue->count = 0;
  this_queue->head = 0;
  this_queue->tail = 0;
  this_queue->receive_wait_head = NULL;
  this_queue->receive_wait_tail = NULL;
  this_queue->send_wait_head = NULL;
  this_queue->send_wait_tail = NULL;

  return FLEURR_OK;
}

fleurr_status_t fleurr_queue_send(const void *item_ptr, queue_handle_t q) {
  uint8_t old_state = port_enter_critical();

  if (q->count == q->capacity) {
    task_handle_t this_task = get_current_task();
    remove_ready_task(this_task);
    this_task->state = TASK_BLOCKED;

    task_handle_t iter = q->send_wait_head;
    task_handle_t prev = NULL;

    while (iter != NULL && iter->priority >= this_task->priority) {
      prev = iter;
      iter = iter->next;
    }
    this_task->prev = prev;
    this_task->next = iter;

    if (prev == NULL) {
      q->send_wait_head = this_task;
    } else {
      prev->next = this_task;
    }
    if (iter == NULL) {
      q->send_wait_tail = this_task;
    } else {
      iter->prev = this_task;
    }

    port_force_context_switch();
    port_exit_critical(old_state);

    old_state = port_enter_critical();
  }

  copy_to_buffer(q, item_ptr);
  unblock_head_task(&q->receive_wait_head, &q->receive_wait_tail);

  port_exit_critical(old_state);
  return FLEURR_OK;
}

fleurr_status_t fleurr_queue_receive(queue_handle_t q, void *receive_buffer) {
  uint8_t old_state = port_enter_critical();

  if (q->count == 0) {
    task_handle_t this_task = get_current_task();
    remove_ready_task(this_task);
    this_task->state = TASK_BLOCKED;

    task_handle_t iter = q->receive_wait_head;
    task_handle_t prev = NULL;

    while (iter != NULL && iter->priority >= this_task->priority) {
      prev = iter;
      iter = iter->next;
    }
    this_task->prev = prev;
    this_task->next = iter;

    if (prev == NULL) {
      q->receive_wait_head = this_task;
    } else {
      prev->next = this_task;
    }
    if (iter == NULL) {
      q->receive_wait_tail = this_task;
    } else {
      iter->prev = this_task;
    }

    port_force_context_switch();
    port_exit_critical(old_state);

    old_state = port_enter_critical();
  }

  copy_from_buffer(q, receive_buffer);
  unblock_head_task(&q->send_wait_head, &q->send_wait_tail);

  port_exit_critical(old_state);
  return FLEURR_OK;
}
