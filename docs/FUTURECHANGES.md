# Future Changes

Things I know need to change but haven't done yet.

## Remove the inline stack array from task_t (COMPLETED)

task_t used to embed the stack buffer (stack[MAX_SIZE]), so every static task paid
for the biggest possible stack. Pulled the array out; task_t now keeps only
stack_pointer, pointing at an external buffer.

Note: the "user supplies the buffer" part of this is being replaced, see below.

## Kernel-carved stack pool

Currently the user supplies stack buffers, and I'm leaving it that way for now
(buffers initialized one after another). This can't guarantee alignment, and the
MPU stack-overflow guard depends on alignment. The user shouldn't have to know
that, so the kernel should own the stacks.

Plan: reserve a static stack pool (its own linker-script section in DTCM).
task_create takes a requested stack size, the kernel rounds it up and carves an
aligned stack from the pool. The user says how big, the kernel handles the rest.

Why:
- Alignment guarantee. 4-byte alignment is not enough. AAPCS needs 8-byte stack
  alignment, and the MPU guard region needs its base 32-byte aligned (an
  exact-fit region would need the base aligned to its own power-of-2 size).
- Overflow detection no longer depends on where the user placed their buffers.
  A per-task guard region works with gaps, and doesn't need adjacency.
- Simpler user API: no manual size math, no alignment macros.

Open decisions:
- Size classes (e.g. 512B / 1K / 2K / 4K) vs. carving arbitrary multiples of the
  guard alignment. Size classes bound waste and allow exact-fit regions.
- Pool exhaustion: task_create returns a Fleur error enum instead of failing later.
- Whether task delete returns the stack to the pool (needs a free strategy).
- Pool placement: DTCM. Resolved by the privilege model — task stacks need to
  sit under the DTCM privileged-only background region so the sliding
  active-task MPU region (region 5) is the only thing that ever grants
  unprivileged write access to any of them. Still need to check which DMA
  masters can reach DTCM before the SPI/DMA SD work, since DMA buffers can't
  follow the stacks there if DTCM isn't DMA-reachable on this part.

## Section-placement macros (.dtcm_bss / .dtcm_data)

Static kernel storage (TCBs, scheduler state, and later static mutex/semaphore/
queue storage) must live in DTCM, not SRAM1 — see ARCHITECTURE.md's placement
rule. Right now this means every call site has to remember to write
`__attribute__((section(".dtcm_bss")))` by hand, with zero compiler or runtime
error if it's forgotten — it just silently compiles into `.bss` (SRAM1) instead,
and that object is unprivileged-RW for every task instead of protected kernel
state. This already bit the M7 port's own example `main.c` during MPU bring-up.

Plan: wrap the section attribute in macros so it's structural instead of
memorized —

```c
DTCM_BSS(task_static_t, task_a_storage);
DTCM_DATA(some_initialized_type, thing);
```

or fold it into `TASK_STATIC_DEFINE`/`QUEUE_STATIC_DEFINE` (see the sizing
macros section below) so DTCM placement happens automatically as part of
declaring the static storage, not as a separate step the caller can skip.
Whichever shape, the goal is: it should not be possible to declare a kernel
object's static storage and have it land in the wrong region without an
explicit, visible opt-out.

## Static storage sizing macros

Buffers are external (queue storage), so the user does size math by hand
(capacity * item_size). Wrap it in macros so the byte math happens at compile time.

```c
QUEUE_STATIC_DEFINE(name, capacity, item_type)
TASK_STATIC_DEFINE(name, stack_size)
```

Change: with the kernel-carved pool, TASK_STATIC_DEFINE no longer defines a stack
buffer. It only reserves the task control block, and stack_size becomes the
request passed to the pool carver (or is dropped from the macro entirely if
size is only given at task_create). The queue macro is unaffected.

Both this and the DTCM placement macros above are ultimately the same kind of
gap (a call site can silently get static allocation wrong), so these may end
up as one combined macro rather than two — worth deciding once both are
actually being written.

Not written yet. Core mechanism needs to be solid first.

## get_current_task() vs. a direct extern pointer

`get_current_task()` currently returns `scheduler.current_task`, where
`scheduler` is a private static inside the kernel's own source file — the
struct itself (ready lists, bitmap, sleep list) is never exposed. Now that
`scheduler` lives in DTCM, every caller of `get_current_task()` from task
context has to go through SVC anyway (DTCM is privileged-only), so the
function-call indirection isn't buying isolation it wasn't already getting
from the section placement.

Considering replacing it with a single exported pointer —

```c
extern task_handle_t *fleurr_current_task_ptr;
```

— written only by the scheduler, read by inline helpers. Narrower surface
than exposing the whole `struct scheduler` (still just the one pointer, nothing
about ready-list internals), and lets call sites like the MPU/priv helpers
become `static inline` without needing the scheduler's internals visible in a
header. Not yet decided against keeping it a real function — mainly a
question of whether the inlining is worth the header-visibility tradeoff once
SVC-wrapping makes most call sites Handler-mode-only anyway.

## Full SVC syscall dispatch

The SVC handler currently does one thing unconditionally (clear nPRIV, return)
regardless of what triggered it. Needs to become a real dispatch: syscall
number in r0 (or similar), handler branches on it. Required for:

- The flagship ELF loader's syscall ABI (write/read/delay/exit), which needs
  pointer validation on every call since the handler runs privileged.
- Routing existing kernel helpers (task_sleep, set_priority, mutex/semaphore/
  queue operations) through SVC instead of calling DTCM-resident kernel state
  directly from task context — currently these only "work" from unprivileged
  code because the DTCM privilege boundary isn't fully closed yet (scheduler
  moved to DTCM, but the helpers that read it haven't been rewritten to go
  through SVC). This is a correctness gap, not a someday-feature: those calls
  will start faulting from unprivileged task code as soon as the DTCM move is
  live, until they're wrapped.

Not started. sys.h / sys.c (public fleurr_raise_priv/fleurr_drop_priv wrappers
around port_raise_priv/port_drop_priv) are being written next and are a
prerequisite for this.

## Per-port static struct sizes

task_static_t is the same byte size on AVR and the M7, even though their task_t
fields are not remotely the same size (M7's carries MPU region cache and a priv
flag that AVR has no equivalent of). AVR wastes space, or the M7 is
shortchanged, depending on which size was picked around.

Plan: make the static storage size a per-port value (e.g. FLEURR_TASK_STATIC_SIZE
in each port's header) so each arch's opaque static storage type is sized to what
that arch needs.

## Port-specific task_t

Common fields (links, priority, state, stack pointer) stay in a shared base. Each
port defines its own task_t (or port_task_t) on top, with whatever extra it needs.
M7 carries MPU region info for stack-overflow protection (guard region base/attrs,
cached region-5 RBAR/RASR, and the priv flag for unprivileged tasks), and AVR
doesn't pay for it. Ties into the per-port static sizes above, since the
port-specific task_t is what the per-port size measures.
