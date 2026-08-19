/*
 * First code executed after the ESP32-S3 ROM loader jumps into our image.
 * This file uses Xtensa call0 ABI; the C files use the same ABI.
 */

    .section .entry.text, "ax"
    .align  4
    .global _start
    .type   _start, @function

_start:
    /* No vector table is installed, so mask all maskable interrupts. */
    rsil    a2, 15

    /* A C function needs a valid downward-growing stack. */
    movi    a1, _stack_top

    /* C requires every byte in .bss to start as zero. */
    movi    a2, _bss_start
    movi    a3, _bss_end
    movi    a4, 0
    beq     a2, a3, .Lbss_done

.Lbss_clear:
    s32i    a4, a2, 0
    addi    a2, a2, 4
    bltu    a2, a3, .Lbss_clear

.Lbss_done:
    memw
    call0   runtime_init
    call0   main

    /* main() is not supposed to return. Stay somewhere safe if it does. */
.Lhang:
    j       .Lhang

    .size _start, . - _start
