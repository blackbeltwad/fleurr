#ifndef FLEURR_SYNC_INTERNAL_H
#define FLEURR_SYNC_INTERNAL_H

// Private — only included by kernel/*.c.

#include "fleurr/config.h"
#include "fleurr/sync.h"
#include "task_internal.h"
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
  uint8_t count;
};

#endif // FLEURR_SYNC_INTERNAL_H
