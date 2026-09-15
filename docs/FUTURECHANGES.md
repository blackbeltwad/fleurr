# Future Changes

Things I know need to change but haven't done yet.

## Remove the inline stack array from task_t — COMPLETED

Right now task_t has the stack buffer built directly into the struct (stack[MAX_SIZE]). That means task_static_t has to be sized for whatever MAX_SIZE is, even if a given task needs way less. Every static task pays for the biggest possible stack whether it uses it or not.

Plan: pull the array out, keep only stack_pointer in task_t, and have it point at a buffer the user supplies separately, same pattern as the queue's buffer. User provides the stack storage at task creation time, sized however big they actually need it, task_t itself stops caring about stack size at all.

This also means task_static_t shrinks down to something closer to fixed size, since the variable-size part (the stack) lives outside it now.

## Static storage sizing macros

Once buffers are external instead of inline (stack, queue storage), the user is doing manual size math themselves (capacity * item_size for queues, whatever the target stack size is for tasks). Want to wrap this in define macros so the user just says how many/how big and the byte math happens at compile time instead of by hand.

Something like:

```c
QUEUE_STATIC_DEFINE(name, capacity, item_type)
TASK_STATIC_DEFINE(name, stack_size)
```

Not written yet. Core mechanism (queue, and the task stack change above) needs to be solid first.

## Per-port static struct sizes

task_static_t is currently the same byte size on AVR and the M7, even though the actual task_t fields (register set, stack pointer width, whatever else is arch-specific) are not remotely the same size on those two targets. AVR is wasting space reserving room for M7-sized fields it doesn't have, or the M7 is getting shortchanged, depending on which one the current size was picked around.

Plan: make the static storage size a per-port value (something like FLEURR_TASK_STATIC_SIZE defined in each port's header) instead of one shared constant, so each arch's opaque static storage type is sized to what that arch actually needs.

## Port-specific task_t

Common fields (links, priority, state, stack pointer) stay in a shared base. Each port defines its own task_t (or port_task_t) on top of that with whatever extra fields it needs, e.g. M7 will carry MPU region info for stack-overflow protection, AVR has no equivalent field and doesn't pay for it. Ties into the per-port static struct sizes change above, since the port-specific task_t is what the per-port size is measuring.
