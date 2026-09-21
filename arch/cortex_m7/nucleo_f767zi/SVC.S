.syntax unified
    .cpu cortex-m7
    .thumb

.global SVC_Handler
.global SVC_Exit
.type SVC_Handler, %function
SVC_Handler:
  mrs R0, CONTROL
  bic R0, R0 #1
  msr CONTROL, R0
  isb
  bx lr

SVC_Exit:
  mrs R0, CONTROL 
  orr R0, R0 #1
  msr CONTROL, R0
  isb
  bx lr
