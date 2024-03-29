#include "arm/sysregs.h"
#include "arm/mmu.h"
#include "mm.h"
#include "peripherals/base.h"

.section ".text.boot"

.globl _start
_start:
    mrs x0, mpidr_el1
    and x0, x0, #0xFF // check processor id
    cbz x0, master // hang for all non-primary CPU
    b proc_hang

proc_hang:
    b proc_hang

master:
    ldr x0, =SCTLR_VALUE_MMU_DISABLED
    msr sctlr_el1, x0

    ldr x0, =HCR_VALUE
    msr hcr_el2, x0

    ldr x0, =SCR_VALUE
    msr scr_el3, x0

    ldr x0, =SPSR_VALUE
    msr spsr_el3, x0

    adr x0, el1_entry
    msr elr_el3, x0
    eret

el1_entry:
    adr x0, bss_begin
    adr x1, bss_end
    sub x1, x1, x0
    bl memzero

    bl __create_page_tables

    mov x0, #VA_START
    add sp, x0, #LOW_MEMORY

    adrp x0, pg_dir
    msr ttbr1_el1, x0

    ldr x0, =(TCR_VALUE)
    msr tcr_el1, x0

    ldr x0, =(MAIR_VALUE)
    msr mair_el1, x0

    ldr x2, =kernel_main

    mov x0, #SCTLR_MMU_ENABLED
    msr sctlr_el1, x0

    br x2

    .macro create_pgd_entry, tbl, virt, tmp1, tmp2
    create_table_entry \tbl, \virt, PGD_SHIFT, \tmp1, \tmp2
    create_table_entry \tbl, \virt, PUD_SHIFT, \tmp1, \tmp2
    .endm

    /* "create_table_entry" responsible for allocating a new page table (In our case either PGD or PUD)
        tbl - a pointer to a memory region were new table has to be allocated.
        virt - virtual address that we are currently mapping.
        shift - shift that we need to apply to the virtual address in order to extract current table index. (39 in case of PGD and 30 in case of PUD) */
    .macro create_table_entry, tbl, virt, shift, tmp1, tmp2
    lsr \tmp1, \virt, #\shift
    and \tmp1, \tmp1, #PTRS_PER_TABLE - 1
    add \tmp2, \tbl, #PAGE_SIZE
    orr \tmp2, \tmp2, #MM_TYPE_PAGE_TABLE
    str \tmp2, [\tbl, \tmp1, lsl #3]
    add \tbl, \tbl, #PAGE_SIZE // next level table page
    .endm

    .macro create_block_map, tbl, phys, start, end, flags, tmp1
    lsr \start, \start, #SECTION_SHIFT
    and \start, \start, #PTRS_PER_TABLE - 1
    lsr \end, \end, #SECTION_SHIFT
    and \end, \end, #PTRS_PER_TABLE - 1
    lsr \phys, \phys, #SECTION_SHIFT
    mov \tmp1, #\flags
    orr \phys, \tmp1, \phys, lsl #SECTION_SHIFT
9999:   str \phys, [\tbl, \start, lsl #3]
    add \start, \start, #1
    add \phys, \phys, #SECTION_SIZE
    cmp \start, \end
    b.ls 9999b
    .endm

__create_page_tables:
    mov x29, x30 // save return address

    /* Init page space clean */
    adrp x0, pg_dir
    mov x1, #PG_DIR_SIZE
    bl memzero

    adrp x0, pg_dir
    mov x1, #VA_START
    create_pgd_entry x0, x1, x2, x3

    /* Mapping kernel and init stack */
    mov x1, xzr // start mapping from physical offset 0
    mov x2, #VA_START // first virtual address
    ldr x3, =(VA_START + DEVICE_BASE - SECTION_SIZE) // last virtual address
    create_block_map x0, x1, x2, x3, MMU_FLAGS, x4

    /* Mapping device memory */
    mov x1, #DEVICE_BASE // start mapping from device base address
    ldr x2, =(VA_START + DEVICE_BASE) // first virtual address
    ldr x3, =(VA_START + PHYS_MEMORY_SIZE - SECTION_SIZE) // last virtual address
    create_block_map x0, x1, x2, x3, MMU_DEVICE_FLAGS, x4

    mov x30, x29 // restore return address
    ret
