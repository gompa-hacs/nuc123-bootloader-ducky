/*
 * Linker script for NUC123 LDROM
 * LDROM: 4KB at 0x00100000
 * SRAM:  12KB at 0x20000000
 *
 * SRAM layout:
 *   0x20000000: .stack  (1KB, grows DOWN from __initial_sp at 0x20000400)
 *   0x20000400: .data   (initialized data, copied from LDROM by startup)
 *   after .data: .bss   (zero-initialized data)
 */

MEMORY
{
    LDROM (rx)  : ORIGIN = 0x00100000, LENGTH = 4K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 12K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector))    /* Vector table must be first */
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > LDROM

    /*
     * Stack MUST come first in SRAM.
     * startup_NUC123_user.s defines __initial_sp at the end of .stack section.
     * ARM stack grows downward from __initial_sp.
     * Placing stack first ensures it never overlaps .data or .bss.
     */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        *(.stack)
        . = ALIGN(8);
    } > SRAM

    /* Heap (empty by default, Heap_Size = 0 in startup) */
    .heap (NOLOAD) :
    {
        *(.heap)
    } > SRAM

    .data :
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > SRAM AT > LDROM

    _sidata = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > SRAM

    /DISCARD/ :
    {
        *(.ARM.exidx*)
    }
}
