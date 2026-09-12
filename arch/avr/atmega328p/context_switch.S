// hey lol
.text
.global __vector_11
.extern store_and_pop_stack_pointer
.extern update_sleep_timer
__vector_11:
  push R0
  push R1
  push R2
  push R3
  push R4
  push R5
  push R6
  push R7
  push R8
  push R9
  push R10
  push R11
  push R12
  push R13
  push R14
  push R15
  push R16
  push R17
  push R18
  push R19
  push R20
  push R21
  push R22
  push R23
  push R24
  push R25
  push R26
  push R27
  push R28
  push R29
  push R30
  push R31
  LDS R25, 0x005f; SREG REGISTER
  push R25
  LDS R25, 0x005E; SPH
  LDS R24, 0x005D; SPL
  CLR R1 
  ; ----------------------------------------------------------------------  
  call store_and_pop_stack_pointer
  CLR R1
  STS 0x005E, R25
  STS 0x005D, R24
  call update_sleep_timer
  CLR R1
  ; ----------------------------------------------------------------------

  pop R0; Store Sreg is R0 only trigger at the end
  STS 0x005f, R0
  pop R31
  pop R30
  pop R29
  pop R28
  pop R27
  pop R26
  pop R25
  pop R24
  pop R23
  pop R22
  pop R21
  pop R20
  pop R19
  pop R18
  pop R17
  pop R16
  pop R15
  pop R14
  pop R13
  pop R12
  pop R11
  pop R10
  pop R9
  pop R8
  pop R7
  pop R6
  pop R5
  pop R4
  pop R3
  pop R2
  pop R1
  pop R0
  SEI
  RETI; SUCCESSFUL CONTEXT SWITCH
