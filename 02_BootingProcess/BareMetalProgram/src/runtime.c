#include "esp32s3_registers.h"

void runtime_init(void)
{
    /* A normal second-stage bootloader performs these steps for us.
     * This project boots directly from ROM, so it must do them itself. */

    REG32(SUPER_WDT_PROTECT_REG) = SUPER_WDT_WRITE_KEY;
    REG32(SUPER_WDT_CONFIG_REG) |= SUPER_WDT_AUTO_FEED_ENABLE;
    REG32(SUPER_WDT_PROTECT_REG) = 0u;

    REG32(RTC_WDT_PROTECT_REG) = WDT_WRITE_KEY;
    REG32(RTC_WDT_CONFIG_REG) &=
        ~(WDT_ENABLE | RTC_WDT_FLASH_BOOT_ENABLE);
    REG32(RTC_WDT_PROTECT_REG) = 0u;

    REG32(TIMER0_WDT_PROTECT_REG) = WDT_WRITE_KEY;
    REG32(TIMER0_WDT_CONFIG_REG) &=
        ~(WDT_ENABLE | TIMER_WDT_FLASH_BOOT_ENABLE);
    REG32(TIMER0_WDT_PROTECT_REG) = 0u;

    __asm__ volatile("memw" ::: "memory");
}

