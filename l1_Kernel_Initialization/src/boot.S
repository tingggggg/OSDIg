#include "mm.h"

.data
barrier: .byte 0
         .align 2

.section ".text.boot"

.global _start
_start:
    mrs x0, mpidr_el1
    and x0, x0, #0xFF // check processor id
    cbz x0, bss_init // Do BSS initialization for CORE #0
    b wait

bss_init:
    // BSS Initialization
    adr x0, bss_begin
    adr x1, bss_end
    sub x1, x1, x0
    bl memzero

    // Tell other CPU that BSS is ready
    mov w0, #1
    ldr x1, =barrier // x1 equal address of barrier
    strb w0, [x1] // write w0 value to barrier(x1)
    b master

wait:
    // Wait for BSS to do initialized
    ldr x1, =barrier
    ldrb w0, [x1]
    cmp w0, #1
    beq master // check barrier equal 1 or not (initialization is complete or not)
    b wait

proc_hang:
    b proc_hang

master:
    // Use the CORE ID ot determint the stack loacation
    // CORE #0: sp = 1 * #LOW_MEMORY, CORE #1: sp = 2 * #LOW_MEMORY
    mov x1, #LOW_MEMORY
    mrs x0, mpidr_el1
    and x0, x0, #0xFF
    add x2, x0, #1
    mul x1, x1, x2
    mov sp, x1

    bl kernel_main
    b proc_hang