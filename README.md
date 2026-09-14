# Fleurr-OS

A bare-metal RTOS kernel, AVR up to Cortex-M7. Zero-cost abstraction, fast primitives, no shortcuts.

## What This Is

A preemptive, priority-based RTOS built from scratch. No HAL, no vendor abstraction layers. Every context switch, scheduling decision, and sync primitive is written and debugged at the register level. Started on the ATmega328P (AVR), now expanding to the NUCLEO-F767ZI (Cortex-M7).

Eventually I want this to feel like the "C++ philosophy" applied to an RTOS. Ergonomic primitives (tasks, mutexes, semaphores, queues) that still compile down to the same performance you'd get writing it all by hand.

## Why This Project Exists

I didn't want a tutorial RTOS where you just copy-paste and it works. I wanted to know exactly what happens when a task blocks, when a context switch fires, when two tasks race on the same resource. So every bug (bad stack layout, priority inversion, non-atomic register access, race conditions) is something I actually dug into instead of patching around.

Really it comes down to two things:

- Memory management. Stack layout, allocation strategies, tradeoffs between them. Not something a library just handles for you.
- The computer itself. What the CPU is actually doing during an interrupt or a context switch or a memory access, not a black box.

## Current Features

- Preemptive priority scheduling on a priority-bucketed ready queue: per-priority intrusive doubly-linked lists, bitmap + CLZ for O(1) highest-priority lookup, up to 32 priority levels
- Separate sleep list, walked once per tick
- Hand-written context switching at the register level
- Mutexes with priority inheritance, with proper multi-mutex support (releasing one mutex falls back to the max of base priority and any remaining held-mutex boosts, not a flat reset)
- Semaphores: no ownership, no inheritance, priority-ordered wait list, direct handoff on signal instead of bump-then-drain
- Queue: ring buffer with separate send/receive wait lists, working
- AVR (ATmega328P) done, Cortex-M7 (NUCLEO-F767ZI) context switching done, sync primitives being ported

## Platforms

| Platform | Architecture | Status |
|---|---|---|
| ATmega328P | AVR | Scheduler, context switching, mutexes, semaphores, queues all working |
| NUCLEO-F767ZI | ARM Cortex-M7 | Scheduler, context switching, mutexes, semaphores, queues all working |

## Recent Bugs

- `task_create_static` was linking a task into its ready bucket before its priority was even set. Ended up in the wrong bucket off garbage data.
- `__builtin_clz` on AVR quietly uses the 16-bit `__clzhi2` helper since AVR's `int` is 16 bits. Wrong results against a 32-bit bitmap. Switched to `__builtin_clzl`, which actually works on a 32-bit `long`.
- Confirmed on hardware with GDB that AVR's `RETI` pops the return address low byte first, not high byte first like I assumed. Had to fix `port_init_stack_frame` for that.
- Stack frame format mismatch between `port_init_stack_frame` and `port_start_first_task`: the first task started fine but any second or third task hit an incomplete fake frame on its first run and jumped to garbage. Fixed by rewriting both to share the same pop+reti restore path.
- On the M7 port I never copied `.data` from its load address to its run address, and never zeroed `.bss`. Globals looked fine until they didn't. Anything relying on zero-init or an initial value from flash was just reading garbage RAM.

Multi-task context switching now works on both AVR and Cortex-M7.

Queue is now working: fixed the head/tail wraparound math, the byte-vs-item count mismatch, the NULL tail dereference on empty-list insert, the non-atomic wake-then-transfer on blocked send/receive, and wait-list nodes not getting fully cleaned up on pop.

## Roadmap

### RTOS Core (Cortex-M7 port)
- ~~ARM context switching via SysTick and PendSV~~
- ~~Runs on PSP from reset~~
- ~~EXC_RETURN handled in the PendSV context switch~~
- ~~Mutexes with priority inheritance (ported and expanded)~~
- ~~Semaphores~~
- ~~Queues~~
- Task delays
- MPU-based task isolation
- Error handling and timeouts

### Drivers (interrupt-driven, not polling)
- UART
- I2C
- SPI
- GY-521 accelerometer
- DS-1307 RTC
- I2C LCD
- SPI SD card module
- SPI OLED

### Later
Timeline's flexible on these, depth matters more than speed:

- Dynamic task allocation
- Static vs dynamic memory allocation schemes, so I can actually mess with allocator design instead of committing to one approach
- Stack overflow detection (guard patterns or MPU guard regions)
- Stack high-water-mark / usage profiling
- Tickless idle / low-power mode

### Planned Refactors
Tracked in `FUTURECHANGES.md`:

- Pull the stack array out of `task_t` into an external user-supplied buffer, same pattern as the queue's buffer
- Sizing macros so users don't have to hand-compute queue capacity/item_size or stack sizing
- Per-port static struct sizes instead of one shared size, since AVR and M7 `task_t` fields differ enough that a shared constant wastes space on one arch or the other

### Beyond the Kernel
- Cache/DMA coherency on Cortex-M7

## Documentation

Documenting as I go. Debugging write-ups, before/after bug demos, design notes, all living alongside the code.

## Design Notes

- This is an RTOS kernel, not a general-purpose OS kernel. Real-time scheduling and sync primitives for embedded targets, not process isolation or virtual memory.
- No HAL, no Arduino abstraction. Drivers and kernel code written directly against datasheets and reference manual.
