#ifndef FLEURR_CONFIG_H
#define FLEURR_CONFIG_H

// User-tunable, compile-time configuration.

#ifndef NUM_OF_PRIORITY
#define NUM_OF_PRIORITY 32
#endif

#ifndef MAX_SIZE
#define MAX_SIZE 256 // per-task stack size, in bytes
#endif

// Sized-but-opaque storage for static allocation. TASK_STATIC_SIZE must be
// kept >= sizeof(struct task) the kernel asserts this at build time.
#ifndef TASK_STATIC_SIZE
#define TASK_STATIC_SIZE 300
#endif

#ifndef PORT_MAX_INTERRUPT_PRIORITY
#define PORT_MAX_INTERRUPT_PRIORITY 6UL
#endif

#endif // FLEURR_CONFIG_H
