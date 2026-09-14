#ifndef FLEURR_QUEUE_H
#define FLEURR_QUEUE_H
#include "status.h"
#include <stddef.h>
#include <stdint.h>
// Not yet implemented — placeholder.

typedef struct queue queue_t;
typedef queue_t *queue_handle_t;

#define QUEUE_STATIC_SIZE 44

typedef struct {
  uint8_t _reserved[QUEUE_STATIC_SIZE];
} queue_static_t;

fleurr_status_t queue_create_static(queue_handle_t *out, size_t item_size,
                                    size_t capacity, queue_static_t *storage,
                                    uint8_t *buffer);

#endif // FLEURR_QUEUE_H
