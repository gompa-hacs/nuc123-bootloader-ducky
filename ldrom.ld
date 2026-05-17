/*
 * Linker script for NUC123 LDROM
 * LDROM: 4KB at 0x00100000
 * SRAM:  12KB at 0x20000000
 *
 * SRAM layout:
 *   0x20000000: .stack  (2KB, grows DOWN from __initial_sp at 0x20000800)
 *   0x20000800: .data   (initialized data, copied from LDROM by startup)
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
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
        _etext = .;
    } > LDROM

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        *(.stack)
        . = ALIGN(8);
    } > SRAM

    /* Do not place .heap — empty heap VMA at 0x20000000 overlaps stack */

    .data ORIGIN(SRAM) + 0x800 :
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

    ASSERT(_sdata >= __initial_sp, "SRAM: .data overlaps stack")
    ASSERT(_ebss <= ORIGIN(SRAM) + LENGTH(SRAM), "SRAM overflow")

    /DISCARD/ :
    {
        *(.ARM.exidx*)
    }
}
