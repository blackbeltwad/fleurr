#include "fleurr/status.h"
#include "fleurr/sync.h"
#include "sync_internal.h"
#include <stddef.h>

// TODO: move existing lock_mutex / unlock_mutex bodies here, adapted to:
//   - return fleurr_status_t
//   - branch on mutex->protocol (PROTOCOL_INHERIT implemented,
//     PROTOCOL_CEILING planned — see docs/ARCHITECTURE.md)

fleurr_status_t mutex_create_static(mutex_handle_t *out,
                                    mutex_protocol_t protocol,
                                    uint8_t ceiling_priority,
                                    mutex_static_t *storage) {
  mutex_t *this_mutex = (mutex_t *)storage;
  *out = this_mutex;

  this_mutex->owner = NULL;
  this_mutex->protocol = protocol;

  if (protocol == PROTOCOL_CEILING) {
    this_mutex->ceiling_priority = ceiling_priority;
  } else {
    this_mutex->ceiling_priority = 0;
  }

  return FLEURR_OK;
}

fleurr_status_t mutex_lock(mutex_handle_t mutex) {}
