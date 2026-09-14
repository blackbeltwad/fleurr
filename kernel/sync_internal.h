#ifndef FLEURR_SYNC_INTERNAL_H
#define FLEURR_SYNC_INTERNAL_H

// Private — only included by kernel/*.c.

#include "fleurr/config.h"
#include "fleurr/sync.h"
#include "task_internal.h"
#include <stddef.h>
#include <stdint.h>

struct mutex {
  struct task *owner;
  struct task *block_head;
  struct task *block_tail;
  mutex_handle_t owner_next;
  mutex_handle_t owner_prev;
  mutex_protocol_t protocol;
  uint8_t ceiling_priority; // only meaningful if protocol == PROTOCOL_CEILING
};

struct semaphore {
  struct task *wait_head;
  struct task *wait_tail;
  uint8_t count;
};

struct queue {
  uint8_t *buffer; // capacity * item_size bytes, ring buffer
  size_t item_size;
  size_t capacity;
  size_t head;  // next slot to read from
  size_t tail;  // next slot to write to
  size_t count; // how many items currently stored

  task_handle_t send_wait_head; // tasks blocked because queue was full
  task_handle_t send_wait_tail;
  task_handle_t receive_wait_head; // tasks blocked because queue was empty
  task_handle_t receive_wait_tail;
};

#endif
// FLEURR_SYNC_INTERNAL_H
