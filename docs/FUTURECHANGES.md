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
- Pool placement: DTCM vs. SRAM. Check which DMA masters can reach DTCM before
  the SPI/DMA SD work, since DMA buffers may have to live elsewhere.

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

Not written yet. Core mechanism needs to be solid first.

## Per-port static struct sizes

task_static_t is the same byte size on AVR and the M7, even though their task_t
fields are not remotely the same size. AVR wastes space, or the M7 is shortchanged,
depending on which size was picked around.

Plan: make the static storage size a per-port value (e.g. FLEURR_TASK_STATIC_SIZE
in each port's header) so each arch's opaque static storage type is sized to what
that arch needs.

## Port-specific task_t

Common fields (links, priority, state, stack pointer) stay in a shared base. Each
port defines its own task_t (or port_task_t) on top, with whatever extra it needs.
M7 carries MPU region info for stack-overflow protection (guard region base/attrs,
and the privilege flag if unprivileged tasks are added), and AVR doesn't pay for it.
Ties into the per-port static sizes above, since the port-specific task_t is what
the per-port size measures.
