# Fleurr

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
- MPU-based unprivileged task isolation on Cortex-M7: tasks run unprivileged by default, SVC is the sole path to raise privilege, a sliding per-task stack region reprograms on every context switch
- Stack overflow detection on Cortex-M7 as a side effect of the isolation model: an unprivileged task's stack region is its only window into DTCM, so overflowing it hits a no-access region and faults immediately instead of silently corrupting a neighbor. Not present on AVR, no hardware to support it there.
- AVR (ATmega328P) done, Cortex-M7 (NUCLEO-F767ZI) context switching done, sync primitives being ported

## Platforms

| Platform | Architecture | Status |
|---|---|---|
| ATmega328P | AVR | Scheduler, context switching, mutexes, semaphores, queues all working. No MPU, no memory protection, no stack overflow detection. |
| NUCLEO-F767ZI | ARM Cortex-M7 | Scheduler, context switching, mutexes, semaphores, queues all working. MPU-based unprivileged task isolation working, with stack overflow detection for unprivileged tasks as a byproduct. |

## Recent Bugs

- `task_create_static` was linking a task into its ready bucket before its priority was even set. Ended up in the wrong bucket off garbage data.
- `__builtin_clz` on AVR quietly uses the 16-bit `__clzhi2` helper since AVR's `int` is 16 bits. Wrong results against a 32-bit bitmap. Switched to `__builtin_clzl`, which actually works on a 32-bit `long`.
- Confirmed on hardware with GDB that AVR's `RETI` pops the return address low byte first, not high byte first like I assumed. Had to fix `port_init_stack_frame` for that.
- Stack frame format mismatch between `port_init_stack_frame` and `port_start_first_task`: the first task started fine but any second or third task hit an incomplete fake frame on its first run and jumped to garbage. Fixed by rewriting both to share the same pop+reti restore path.
- On the M7 port I never copied `.data` from its load address to its run address, and never zeroed `.bss`. Globals looked fine until they didn't. Anything relying on zero-init or an initial value from flash was just reading garbage RAM.
- M7 linker script had SRAM1's origin overlapping DTCM's address range. The MPU region built from it rounded down to a base that covered both, so the region meant to grant unprivileged access to `.data`/`.bss` was also granting unprivileged access to all of DTCM: TCBs, stacks, kernel objects. Fixed the origin and carved the overlap back out with the region's SRD (subregion disable) bits.
- `.dtcm_bss` is declared NOLOAD in the linker script but nothing ever actually zeroed it, unlike `.bss`. Every static object placed there (TCBs, the scheduler struct, task stacks) booted with whatever was already sitting in DTCM instead of a known state. Surfaced as `task->priv` reading garbage on boot and breaking `port_restore_priv`. Added a zero loop for `.dtcm_bss` in `Reset_Handler` next to the existing `.bss` one, and made `task_create_static` set `priv` explicitly instead of relying on zero-fill for that field.

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
- ~~MPU-based task isolation: unprivileged tasks, sliding active-task region, SVC as the sole privilege boundary~~ (see `ARCHITECTURE.md`)
- ~~Stack overflow detection for unprivileged tasks~~ (a consequence of the isolation model, not a separate mechanism)
- Task delays
- SVC syscall interface, and routing existing kernel helpers (`task_sleep`, `set_priority`, mutex/semaphore/queue ops) through it now that unprivileged tasks can't reach kernel state directly
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
- Tickless idle / low-power mode

Dropped for now: stack high-water-mark / usage profiling. Not worth the time against everything else on the list. Might revisit if there's free time later.

### Planned Refactors
Tracked in `FUTURECHANGES.md`:

- Pull the stack array out of `task_t` into an external user-supplied buffer, same pattern as the queue's buffer (done, now being replaced by a kernel-carved stack pool, see `FUTURECHANGES.md`)
- Sizing macros so users don't have to hand-compute queue capacity/item_size or stack sizing
- Section-placement macros so kernel-private static storage can't silently land in the wrong memory region
- Per-port static struct sizes instead of one shared size, since AVR and M7 `task_t` fields differ enough that a shared constant wastes space on one arch or the other

### Beyond the Kernel
- Cache/DMA coherency on Cortex-M7

## Documentation

Documenting as I go. Debugging write-ups, before/after bug demos, design notes, all living alongside the code.

## Design Notes

- This is an RTOS kernel, not a general-purpose OS kernel. Real-time scheduling and sync primitives for embedded targets, not process isolation or virtual memory.
- No HAL, no Arduino abstraction. Drivers and kernel code written directly against datasheets and reference manual.
- On Cortex-M7, tasks run unprivileged; the MPU and an SVC-gated privilege boundary are the actual isolation mechanism, not a convention. Stack overflow detection for unprivileged tasks falls out of that same boundary rather than a separate guard region. See `ARCHITECTURE.md`.
- No stack overflow detection on AVR. There's no MPU or equivalent protection hardware on the 328P, so it would need a software canary/guard pattern checked on switch, which hasn't been added.
