#ifndef FLEURR_PORT_CORTEX_M7_NUCLEO_F767ZI_H
#define FLEURR_PORT_CORTEX_M7_NUCLEO_F767ZI_H
#define POP_SIZE 32
#define WORD_SIZE 4
#define CALLER_SAVED 28
#define INIT_POP POP_SIZE + CALLER_SAVED - WORD_SIZE
#define ALLIGN_IT 3
#define ARG_SIZE NULL
// Arch-specific interface that kernel/*.c calls into. Keeps
// kernel/scheduler.c free of #ifdefs for arch-specific behavior.
#include "fleurr/task.h"
#include "svc.h"
#include "task_internal.h"
#include <stdint.h>

extern uint8_t global_mpu_value;
void port_start_first_task(void);
void port_timer_init(uint32_t interval_ms);
void port_force_context_switch(void); // maps to existing task_yield() body
void port_init_stack_frame(uint8_t **stack_pointer, void (*entry)(void *),
                           void *arg);
void port_context_switch(task_handle_t out, task_handle_t in);
uint8_t port_enter_critical(void);
void port_exit_critical(uint8_t old_state);
fleurr_status_t port_mpu_configuration(task_handle_t this_task,
                                       size_t capacity);
void port_apply_active_task_region(task_handle_t this_task);
void port_restore_priv();
void fleurr_drop_priv();
void fleurr_raise_priv();
#endif // FLEURR_PORT_CORTEX_M7_NUCLEO_F767ZI_H
