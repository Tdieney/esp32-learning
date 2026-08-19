/*
 * Minimal RAM-only memory layout for a raw ESP32-S3 boot image.
 *
 * These addresses follow the safe RAM ranges used by the ESP-IDF v5.2
 * ESP32-S3 second-stage bootloader. IRAM and DRAM are aliases into the same
 * physical SRAM, so the chosen ranges intentionally do not overlap.
 */

OUTPUT_ARCH(xtensa)
ENTRY(_start)

MEMORY
{
    iram (rx)  : ORIGIN = 0x403C9700, LENGTH = 0x00003000
    dram (rw)  : ORIGIN = 0x3FCE3700, LENGTH = 0x00004000
}

/* Reserve the following 8 KiB for the downward-growing startup stack. */
_stack_bottom = 0x3FCE7700;
_stack_top    = 0x3FCE9700;

SECTIONS
{
    /* VECBASE only keeps the bits above the low 10, so the exception vector
     * table has to start on a 1 KiB boundary. */
    .vectors : ALIGN(0x400)
    {
        _vectors_start = ABSOLUTE(.);
        KEEP(*(.vectors.text))
        _vectors_end = ABSOLUTE(.);
    } > iram

    .iram.text : ALIGN(16)
    {
        _text_start = ABSOLUTE(.);
        KEEP(*(.entry.text))
        *(.literal)
        *(.literal.*)
        *(.text)
        *(.text.*)
        _text_end = ABSOLUTE(.);
    } > iram

    .dram.data : ALIGN(16)
    {
        _data_start = ABSOLUTE(.);
        *(.rodata)
        *(.rodata.*)
        *(.data)
        *(.data.*)
        *(.sdata)
        *(.sdata.*)
        _data_end = ABSOLUTE(.);
    } > dram

    .dram.bss (NOLOAD) : ALIGN(16)
    {
        _bss_start = ABSOLUTE(.);
        *(.bss)
        *(.bss.*)
        *(.sbss)
        *(.sbss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = ABSOLUTE(.);
    } > dram

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.xtensa.info)
        *(.xt.prop)
        *(.xt.lit)
    }
}

ASSERT(_text_end <= ORIGIN(iram) + LENGTH(iram), "IRAM overflow")
ASSERT(_bss_end <= ORIGIN(dram) + LENGTH(dram), "DRAM overflow")

