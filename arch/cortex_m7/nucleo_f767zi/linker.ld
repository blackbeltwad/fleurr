ENTRY(Reset_Handler)

MEMORY
{
    FLASH       (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    DTCM        (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
    SRAM1       (rwx) : ORIGIN = 0x20020000, LENGTH = 368K
    PERIPHERAL  (rwx) : ORIGIN = 0x40000000, LENGTH = 512M
    PERIPHERALX (rwx) : ORIGIN = 0x0A000000, LENGTH = 8K
}

/* MSP Top at end of DTCM */
_estack = ORIGIN(DTCM) + LENGTH(DTCM);

_flash_start = ORIGIN(FLASH);
_dtcm_start  = ORIGIN(DTCM);
_sram1_start = ORIGIN(SRAM1);
_porigin     = ORIGIN(PERIPHERAL);
_pxorigin    = ORIGIN(PERIPHERALX);

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* DTCM KERNEL SPACE (PRIV RW ONLY) */

    /* Initialized kernel data (TCBs, static mutexes/queues/semaphores) */
    .dtcm_data :
    {
        . = ALIGN(4);
        _sdtcm_data = .;
        *(.dtcm_data)
        *(.dtcm_data*)
        . = ALIGN(4);
        _edtcm_data = .;
    } > DTCM AT> FLASH

    _sidtcm_data = LOADADDR(.dtcm_data);

    /* Zero-initialized kernel data */
    .dtcm_bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sdtcm_bss = .;
        *(.dtcm_bss)
        *(.dtcm_bss*)
        . = ALIGN(4);
        _edtcm_bss = .;
    } > DTCM


    /* SRAM1: USER SPACE (UNPRIV ACCESSIBLE) */

    _sidata = LOADADDR(.data);

    /* Global user variables */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM1 AT> FLASH

    /* Zero initialized user variables */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > SRAM1

    /* Dedicated boot/default task PSP stack space in SRAM1 */
    .user_stack (NOLOAD) :
    {
        . = ALIGN(8);
        _suser_stack = .;
        . = . + 0x1000; /* 4KB initial stack allocation */
        . = ALIGN(8);
        _euser_stack = .;
    } > SRAM1
}
