/*
 * Zarqaa OS 0.1 - Kernel Linker Script
 * Creator: Abdelhak Chabbi
 *
 * This linker script places the kernel at the higher-half virtual address
 * 0xFFFFFFFF80000000. The Limine bootloader maps the kernel here before
 * transferring control.
 *
 * Section layout:
 *   .limine_requests  - Limine boot protocol request structures
 *   .text             - Executable code
 *   .rodata           - Read-only data (constants, strings, font data)
 *   .data             - Initialized read-write data
 *   .bss              - Zero-initialized data
 *
 * Each section is page-aligned (4KB) for proper memory protection.
 */

OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

PHDRS
{
    limine_requests PT_LOAD;
    text            PT_LOAD FLAGS((1 << 0) | (1 << 2));     /* PF_X | PF_R */
    rodata          PT_LOAD FLAGS((1 << 2));                 /* PF_R */
    data            PT_LOAD FLAGS((1 << 1) | (1 << 2));     /* PF_W | PF_R */
}

SECTIONS
{
    /* Kernel is loaded at the higher-half virtual address */
    . = 0xFFFFFFFF80000000;

    _kernel_start = .;

    /*
     * Limine requests section — the bootloader scans this section
     * for request structures between the start and end markers.
     */
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    /* Executable code */
    . = ALIGN(4K);
    .text : {
        *(.text .text.*)
    } :text

    /* Read-only data (strings, font bitmaps, etc.) */
    . = ALIGN(4K);
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    /* Initialized read-write data */
    . = ALIGN(4K);
    .data : {
        *(.data .data.*)
    } :data

    /* Zero-initialized data */
    . = ALIGN(4K);
    .bss : {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    } :data

    _kernel_end = .;

    /* Discard unnecessary sections */
    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
        *(.comment)
        *(.gnu.hash)
        *(.gnu.version*)
    }
}
