ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    DTCM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
    PERIPHERAL (rwx) : ORIGIN = 0x40000000
    PERIPHERALX (rwx) : ORIGIN = 0xA000000
}

/* Place MSP Stack at top of 128KB DTCM RAM for maximum CPU execution speed */
_estack = ORIGIN(DTCM) + LENGTH(DTCM);
_flash_start = ORIGIN(FLASH);
_eorigin = ORIGIN(DTCM);
_porigin = ORIGIN(PERIPHERAL);
_pxorigin = ORIGIN(PERIPHERALX);

SECTIONS
{
    /* Vector Table placed at start of Flash */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /* Program Code and Read-Only Constants */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)         /* glue arm to thumb code */
        *(.glue_7t)        /* glue thumb to arm code */
        *(.eh_frame)

        . = ALIGN(4);
        _etext = .;        /* End of code in Flash */
    } > FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* Symbol used by Reset_Handler to locate initial values in Flash */
    _sidata = LOADADDR(.data);

    /* Initialized Globals: Placed in DTCM for maximum performance */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;        /* Start of .data in RAM */
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;        /* End of .data in RAM */
    } > DTCM AT> FLASH

    /* Uninitialized Globals: Allocated in DTCM, cleared to 0 at boot */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;         /* Start of .bss in RAM */
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;         /* End of .bss in RAM */
    } > DTCM
}
