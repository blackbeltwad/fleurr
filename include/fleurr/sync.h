#ifndef FLEURR_SYNC_H
#define FLEURR_SYNC_H

#include "status.h"
#include "task.h"
#include <stdint.h>

// ---- Mutex ----

typedef struct mutex
    mutex_t; // opaque — real definition in kernel/sync_internal.h
typedef mutex_t *mutex_handle_t;

#define MUTEX_STATIC_SIZE 24

typedef struct {
  uint8_t _reserved[MUTEX_STATIC_SIZE];
} mutex_static_t;

typedef enum {
  PROTOCOL_INHERIT, // implemented
  PROTOCOL_CEILING, // planned, not yet implemented
} mutex_protocol_t;

fleurr_status_t
mutex_create(mutex_handle_t *out, mutex_protocol_t protocol,
             uint8_t ceiling_priority /* unused if PROTOCOL_INHERIT */);

fleurr_status_t mutex_create_static(mutex_handle_t *out,
                                    mutex_protocol_t protocol,
                                    uint8_t ceiling_priority,
                                    mutex_static_t *storage);

fleurr_status_t fleurr_mutex_lock(mutex_handle_t mutex);
fleurr_status_t fleurr_mutex_unlock(mutex_handle_t mutex);

// ---- Semaphore ----
// Not yet implemented — placeholder for API shape.

typedef struct semaphore semaphore_t;
typedef semaphore_t *sem_handle_t;

#define SEM_STATIC_SIZE 32
typedef struct {
  uint8_t _reserved[SEM_STATIC_SIZE];
} sem_static_t;

fleurr_status_t sem_create_static(sem_handle_t *out, uint8_t initial_count,
                                  sem_static_t *storage);

fleurr_status_t fleurr_sem_signal(sem_handle_t this_sem);
fleurr_status_t fleurr_sem_wait(sem_handle_t this_sem);
#endif // FLEURR_SYNC_H
