.syntax unified
    .cpu cortex-m7
    .thumb

.global PendSV_Handler
.type PendSV_Handler, %function
PendSV_Handler:
    mrs r0, psp
    stmdb r0!, {r4-r11}
    push {r4, lr}

    bl store_and_pop_stack_pointer
    bl port_mpu_swap

    pop {r4, lr}
    ldmia r0!, {r4-r11}
    msr psp, r0
    bx lr
