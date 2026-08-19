/*
 * First code executed after the ESP32-S3 ROM loader jumps into our image.
 * This file uses Xtensa call0 ABI; the C files use the same ABI.
 */

/* PS fields. WOE stays 0 because everything here is built for call0. */
#define PS_INTLEVEL_MASKED (15)
#define PS_UM              (1 << 5)

/* EXCCAUSE value the CPU reports when a level-1 interrupt is taken. */
#define EXCCAUSE_LEVEL1_INTERRUPT (4)

/* Bytes of interrupted state saved on the stack by _exception_entry. */
#define EXC_FRAME_SIZE (64)

    .section .entry.text, "ax"
    .align  4
    .global _start
    .type   _start, @function

_start:
    /* Start from a known PS instead of whatever the ROM left behind:
     * interrupts masked, user mode (so exceptions use the user vector),
     * not in exception mode. */
    movi    a2, PS_UM | PS_INTLEVEL_MASKED
    wsr     a2, ps
    rsync

    /* Use our own vector table instead of the ROM one. */
    movi    a2, _vector_table
    wsr     a2, vecbase
    rsync

    /* No CPU interrupt line may fire until a driver asks for it. */
    movi    a2, 0
    wsr     a2, intenable
    rsync

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

/*
 * Exception and interrupt vector table.
 *
 * VECBASE must be 1 KiB aligned, which the linker script gives this section.
 * The CPU jumps to a fixed offset inside the table depending on what happened;
 * only the two entries this program can reach are filled in.
 */
    .section .vectors.text, "ax"
    .align  4
    .global _vector_table
    .type   _vector_table, @object

_vector_table:

    /* 0x300 KernelExceptionVector: used when PS.UM == 0. */
    .org    0x300
    j       _exception_entry

    /* 0x340 UserExceptionVector: used when PS.UM == 1. Every level-1
     * interrupt lands here too, flagged by EXCCAUSE == 4. */
    .org    0x340
    j       _exception_entry

    /* Pad out the full 1 KiB table. */
    .org    0x400

    .size _vector_table, . - _vector_table

/*
 * Low-level entry taken from the vectors above.
 *
 * On entry PS.EXCM is already set, so interrupts stay masked and this handler
 * can never nest. It saves the state a call0 C function is allowed to clobber,
 * calls the C dispatcher, then returns to the interrupted instruction.
 */
    .section .iram.text, "ax"
    .align  4
    .global _exception_entry
    .type   _exception_entry, @function

_exception_entry:
    addi    a1, a1, -EXC_FRAME_SIZE
    s32i    a0, a1, 0
    s32i    a2, a1, 4
    s32i    a3, a1, 8
    s32i    a4, a1, 12
    s32i    a5, a1, 16
    s32i    a6, a1, 20
    s32i    a7, a1, 24
    s32i    a8, a1, 28
    s32i    a9, a1, 32
    s32i    a10, a1, 36
    s32i    a11, a1, 40

    /* a12-a15 are callee-saved in call0, the C function preserves them. */

    rsr     a2, sar
    s32i    a2, a1, 44

    /* Zero-overhead loop state: the interrupted code may be inside a hardware
     * loop, and any loop in our own C code would overwrite these. */
    rsr     a2, lbeg
    s32i    a2, a1, 48
    rsr     a2, lend
    s32i    a2, a1, 52
    rsr     a2, lcount
    s32i    a2, a1, 56

    /* Only EXCCAUSE 4 is an interrupt. Anything else is a real fault that
     * this minimal runtime has no way to repair. */
    rsr     a2, exccause
    bnei    a2, EXCCAUSE_LEVEL1_INTERRUPT, .Lfatal

    call0   interrupt_dispatch

    l32i    a2, a1, 48
    wsr     a2, lbeg
    l32i    a2, a1, 52
    wsr     a2, lend
    l32i    a2, a1, 56
    wsr     a2, lcount
    l32i    a2, a1, 44
    wsr     a2, sar

    l32i    a0, a1, 0
    l32i    a2, a1, 4
    l32i    a3, a1, 8
    l32i    a4, a1, 12
    l32i    a5, a1, 16
    l32i    a6, a1, 20
    l32i    a7, a1, 24
    l32i    a8, a1, 28
    l32i    a9, a1, 32
    l32i    a10, a1, 36
    l32i    a11, a1, 40
    addi    a1, a1, EXC_FRAME_SIZE

    /* Clears PS.EXCM and resumes at the address held in EPC1. */
    rfe

.Lfatal:
    j       .Lfatal

    .size _exception_entry, . - _exception_entry
