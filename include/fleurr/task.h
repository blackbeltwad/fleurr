#ifndef FLEURR_TASK_H
#define FLEURR_TASK_H

#include "config.h"
#include "status.h"
#include <stdint.h>

// Opaque pointer
typedef struct task task_t;
typedef struct task *task_handle_t;

// Placeholder value above always verify when modifying task
typedef struct {
  uint8_t _reserved[TASK_STATIC_SIZE];
} task_static_t;

fleurr_status_t task_create(task_handle_t *out, void (*entry)(void *),
                            uint8_t priority, void *arg);

fleurr_status_t task_create_static(task_handle_t *out, void (*entry)(void *),
                                   uint8_t priority, void *arg,
                                   task_static_t *storage);

uint8_t fleurr_enter_critical();
void fleurr_exit_critical(uint8_t oldstate);
void task_yield(void);
void task_block(task_handle_t task);
void task_unblock(task_handle_t task);
void task_sleep(uint32_t time_ms);
void set_priority(task_handle_t task, uint8_t priority);
task_handle_t get_current_task(void);

#endif // FLEURR_TASK_H
