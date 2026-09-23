# Fleurr Architecture

This document describes how Fleurr is structured internally, the design principles behind that structure, and how the current design leaves room for planned future work without requiring a rework later.

## Design Principles

1. **The user never sees kernel internals.** Every kernel object (task, mutex, semaphore, ...) is exposed to user code only as an opaque handle. Real struct definitions live in internal headers that user code never includes.
2. **Static and dynamic allocation are interchangeable from the user's perspective.** Whether a task's memory comes from the caller (static) or the kernel (dynamic, future work), the user gets back the same handle type and uses the same API from that point on.
3. **No abstraction should cost hardware control or performance.** Every primitive is designed to compile down to direct, predictable register-level operations, the same standard applied to the hand-written AVR context switch and mutex code.
4. **Architecture decisions made now should not block features planned for later.** Where a future feature (e.g. priority ceiling protocol) would require a struct field or API shape that's cheap to add now and expensive to retrofit later, that field/shape is included now even if unused.

## Opaque Handle Pattern

All kernel objects follow the same public/private split:

- A **public header** (e.g. `task.h`, `sync.h`) forward-declares the type and exposes only a handle (`typedef struct task task_t; typedef task_t *task_handle_t;`) plus a set of functions that operate on that handle.
- An **internal header** (e.g. `task_internal.h`), included only by the kernel's own source files, contains the real struct definition.

User code interacts with kernel objects exclusively through handles and API calls. It cannot read or write a struct field directly, because it never has access to a type that defines one. Every mutation goes through a kernel function, which means every mutation can be wrapped in the atomicity guarantees (`cli()`/`sei()` on AVR, equivalent primitives on Cortex-M7) the kernel actually needs to preserve its invariants, something direct field access would bypass entirely.

On Cortex-M7 this pattern is no longer just a style convention. See Privilege Model below. Once tasks run unprivileged, the opaque boundary and the hardware privilege boundary line up: kernel objects live in memory unprivileged code cannot reach at all, so going through the kernel API isn't just good practice, it's the only path that works.

## Privilege Model (Cortex-M7)

Tasks run unprivileged by default. Privilege is not a property assigned once at task creation. It's live hardware state (`CONTROL.nPRIV`) that a task can temporarily raise and must drop again, and the kernel has to track it across context switches the same way it tracks a stack pointer.

**Why unprivileged by default.** The MPU only means something if the boundary it enforces can't be walked around. If task code ran privileged, it could write any MPU register directly and grant itself access to anything, and the isolation this architecture exists to provide would be cosmetic. Unprivileged execution is the actual mechanism; everything else here exists to make that survivable.

**Raising privilege is gated through SVC, not a direct register write.** ARMv7-M does not allow unprivileged Thread-mode code to clear `CONTROL.nPRIV` via a plain `MSR`. That's not an oversight, it's the whole basis of the privilege model. The only legal path up is through an exception: `svc` traps into Handler mode (always privileged, unconditionally, by hardware), the handler clears `nPRIV`, and the change persists into Thread mode on return. Dropping privilege has no such restriction. A task can give up privilege with a direct `MSR` at any time, since voluntarily narrowing your own access needs no gate.

```c
void fleurr_raise_priv(void);  // fleurr/sys.h, traps through SVC
void fleurr_drop_priv(void);   // fleurr/sys.h, direct MSR
```

`port_raise_priv()` and `port_drop_priv()` are the arch-level implementations these wrap; user code never calls the `port_*` versions directly, same opaque-boundary rule as everywhere else in this doc.

**Privilege survives a context switch by being saved and restored like any other per-task state.** `CONTROL` is not part of the automatically-stacked exception frame, so if a task is preempted mid-`fleurr_raise_priv()`, nothing about the exception hardware preserves that. Each `struct task` carries a `priv` field. On every switch, the scheduler reads live `CONTROL.nPRIV` from the outgoing task and stores it, then writes the incoming task's saved value back to `CONTROL` before returning to it (`port_restore_priv()`). This makes `priv` a mirror of hardware truth rather than a cache that can drift from it. It's re-derived at every switch, not just updated by `raise`/`drop` themselves, which is what keeps a switch landing mid-transition from leaving the kernel confused about which privilege a task should resume at.

**Not yet done.** The SVC dispatch itself is currently a single unconditional handler (elevate and return) with no syscall number handling. The design target is `r0` carrying a syscall number and the handler dispatching on it: `write`/`read`/`delay`/`exit` for the flagship loader, plus whatever the kernel's own helper functions (`task_sleep`, `set_priority`, mutex/semaphore/queue ops) end up needing once they're routed through SVC instead of calling privileged-only kernel state directly. That routing is required, not optional, once `struct scheduler` moves into DTCM (see Memory Layout below). Today those helpers happen to work from unprivileged task code by accident, because the scheduler isn't actually behind a privilege boundary yet.

## Memory Layout (Cortex-M7)

Five regions, indexed by MPU region number, four static and one that gets reprogrammed on every context switch:

| Region | Contents | Priv | Unpriv | Exec |
|---|---|---|---|---|
| 0, Flash | `.text`, `.rodata`, vector table | RO | RO | Yes |
| 1, DTCM (background) | TCBs, scheduler state, sync primitives, all task stacks | RW | none | No |
| 2, SRAM1 | `.data`, `.bss`, ordinary globals | RW | RW | No |
| 3/4, Peripherals | MMIO | RW | none | No |
| 5, Active task | whichever task is currently running | RW | RW | No |

**DTCM holds every task's stack, not just kernel objects, and that's the actual isolation mechanism, not a static per-task allow-list.** Region 1 is a background grant covering all of DTCM as privileged-only. Region 5 is a single, reprogrammed-per-switch window that slides to cover only the currently active task's stack, with a higher region number than region 1. Higher region numbers win on overlap, so only the active task's stack is ever unprivileged-writable at any given moment. Every other task's stack, still sitting in that same DTCM range, falls back to region 1's privileged-only grant and faults if the active task's code somehow reaches into it. One MPU region slot serves every task, rather than burning one slot per task. This is the FreeRTOS-MPU-style pattern, chosen because Cortex-M7's region count (8, per RM0410 for this part) doesn't scale to one-per-task on top of the other four fixed regions.

**SRAM1 is deliberately unprivileged-RW for both, with no per-task isolation.** `.data`/`.bss` are shared global state by design, not something the architecture tries to wall tasks off from each other. If a global needs protecting, it doesn't belong in `.data`/`.bss`; it belongs in a kernel-managed object (mutex, queue) or DTCM.

**Peripherals have no unprivileged access at all, not read-only.** The design point of routing everything through SVC is that SVC is the only kernel boundary. A read-only escape hatch for MMIO would be a second, ungated one.

### Placement rule: DTCM vs. SRAM1

Anything a task's handle points to that isn't meant to be a shared global (the TCB itself, task stacks, and once implemented, static mutex/semaphore/queue storage) must be placed in DTCM, not left to fall into default `.bss` (which resolves to SRAM1). This is not enforced by the kernel today. It's a per-call-site discipline requirement on whoever allocates static storage, which is exactly the kind of thing that's easy to get wrong silently: a forgotten section attribute compiles clean and just quietly defeats isolation for that object. Macros to close this gap are planned. See `FUTURECHANGES.md`.

## St[118;1:3uatic vs. Dynamic Allocation

Because user code never sees real struct layout, static allocation can't hand the user a real `struct task` to declare on the stack or in `.bss`. They'd need the type definition to size it. Instead, the public header exposes a **sized-but-opaque** buffer type:

```c
typedef struct {
    uint8_t _reserved[TASK_STATIC_SIZE];
} task_static_t;
```

`TASK_STATIC_SIZE` is a public constant guaranteed (via a compile-time `static_assert` inside the kernel) to be large enough to hold the real, private `struct task`. The user can declare `static task_static_t storage;` and pass its address to `task_create_static()`; the kernel initializes the real struct into that memory internally. This keeps the two allocation paths symmetric:

```c
fleurr_status_t task_create(task_handle_t *out,
                             void (*entry)(void *), uint8_t priority, void *arg);

fleurr_status_t task_create_static(task_handle_t *out,
                                    void (*entry)(void *), uint8_t priority, void *arg,
                                    task_static_t *storage);
```

Both return the same `task_handle_t`. Every other kernel function (`task_yield`, `set_priority`, mutex operations, etc.) operates on that handle identically regardless of which path created it. This is what allows dynamic task allocation (planned, not yet implemented) to be added later without changing any existing call site that already uses static allocation.

The same pattern applies to mutexes (`mutex_static_t`) and will apply to semaphores once they're implemented.

On Cortex-M7, static storage created this way additionally has to land in the correct memory region (DTCM, not SRAM1). See the placement rule above. This is currently the caller's responsibility at each call site, same as it is for `task_static_t`/`mutex_static_t` sizing; both are candidates for the same kind of macro-enforced correctness (see `FUTURECHANGES.md`).

## Kernel Objects

### Task

Public: `task_handle_t` (opaque), `task_static_t` (sized opaque storage for static allocation).

Private (`struct task`, internal only): stack pointer, stack buffer, current/base priority, state, sleep remaining, task argument, mutex-ownership bookkeeping, and on Cortex-M7: cached MPU region-5 values (`RBAR`/`RASR`) for this task's stack, and the live `priv` field described in Privilege Model above.

The split between `priority` (effective, scheduler-visible) and `base_priority` (the task's actual assigned priority) exists specifically to support priority inheritance. The scheduler always reads the effective value, while the base value is what gets restored once a boost is no longer justified.

### Scheduler

Unlike task/mutex/semaphore, there is exactly one scheduler instance system-wide. It doesn't need a handle, static/dynamic choice, or public struct at all. It lives as a private object inside the kernel's own source file, placed in DTCM like all other kernel-private state on Cortex-M7, and exposed only through functions (`scheduler_start()`, `get_current_task()`, and later, read-only status/statistics functions that return plain values rather than pointers into internal state).

`get_current_task()` may be replaced by a directly-exported pointer (`extern task_handle_t *fleurr_current_task_ptr`, written only by the scheduler) rather than a function call. See `FUTURECHANGES.md`. This is under consideration specifically because DTCM placement already means most callers reach it only from a privileged context (Handler mode, or after `fleurr_raise_priv()`), so the function-call indirection isn't buying additional isolation beyond what the section placement already provides.

### Mutex

Public: `mutex_handle_t` (opaque), `mutex_static_t` (sized opaque storage).

Private (`struct mutex`, internal only): owner, list of blocked waiters, and a `protocol` field.

The `protocol` field is included now, even though only one protocol is currently implemented, so that adding a second protocol later is a matter of adding a branch inside `lock_mutex`/`unlock_mutex`, not changing the struct shape or any existing call site:

```c
typedef enum {
    PROTOCOL_INHERIT,
    PROTOCOL_CEILING
} mutex_protocol_t;

fleurr_status_t mutex_create(mutex_handle_t *out, mutex_protocol_t protocol,
                              uint8_t ceiling_priority /* unused if PROTOCOL_INHERIT */);
```

**Currently implemented, `PROTOCOL_INHERIT`:** reactive priority inheritance. When a task blocks on a mutex held by a lower-priority task, the holder is boosted to the blocker's priority for the duration it holds the lock, then restored on unlock. Scoped, for now, to a single mutex held at a time. A task holding multiple mutexes simultaneously, and the transitive/chained boosting that implies, is explicitly deferred rather than partially implemented.

**Planned, not yet implemented, `PROTOCOL_CEILING`:** priority ceiling protocol. Each such mutex is created with a fixed ceiling priority (the highest priority of any task that could ever lock it); a task locking the mutex is boosted to that ceiling immediately on lock, proactively, rather than only once real contention appears. This gives a stronger, provable bound on blocking time than reactive inheritance, at the cost of the caller needing to know the ceiling value up front.

Protocol selection is per-mutex, decided at creation time. A single mutex uses one protocol or the other, never both.

## Error Handling

Kernel functions that can fail return a status enum rather than silently succeeding or asserting:

```c
typedef enum {
    FLEURR_OK = 0,
    FLEURR_ERR_NOMEM,
    FLEURR_ERR_INVALID_ARG,
    FLEURR_ERR_LIMIT_REACHED,
    FLEURR_MUTEX_IN_USE,
    FLEURR_MUTEX_NOT_OWNER,
    // extended as new failure modes are identified
} fleurr_status_t;
```

## Planned, Architecturally-Anticipated Work

These are not being built now, but the design above is deliberately kept compatible with them so they don't require restructuring existing code when their time comes:

- **Dynamic task allocation.** A kernel-owned allocator behind `task_create()`, symmetric with the existing static path.
- **Priority ceiling protocol.** The second branch of the `protocol` field described above.
- **Kernel-carved stack pool.** Replacing user-supplied stack buffers with kernel-owned, alignment-guaranteed allocation. See `FUTURECHANGES.md`.
- **Full SVC syscall dispatch.** Numbered syscalls rather than a single unconditional handler, and routing the existing kernel helpers through it. See Privilege Model above and `FUTURECHANGES.md`.
- **Section-placement macros.** Closing the DTCM-vs-SRAM1 placement gap described above. See `FUTURECHANGES.md`.
- **Better dynamic memory management.** Allocator strategy not yet decided; whatever is chosen sits behind the same `task_create()`/`mutex_create()` entry points and doesn't change their public signatures.
