#ifndef ESP32S3_REGISTERS_H
#define ESP32S3_REGISTERS_H

#include <stdint.h>

#define BIT(n) (1u << (n))
#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))

/* GPIO registers used by the LED example. */
#define GPIO_OUT_W1TS_REG          0x60004008u
#define GPIO_OUT_W1TC_REG          0x6000400Cu
#define GPIO_ENABLE_W1TS_REG       0x60004024u
#define GPIO_FUNC4_OUT_SEL_REG     0x60004564u
#define GPIO_MATRIX_SIMPLE_OUTPUT  256u

/* IO MUX register for GPIO4. Function 1 selects ordinary GPIO. */
#define IO_MUX_GPIO4_REG           0x60009014u
#define IO_MUX_FUNCTION_MASK       (7u << 12)
#define IO_MUX_FUNCTION_GPIO       (1u << 12)

/* Registers used by runtime_init() to finish ROM flash-boot housekeeping. */
#define RTC_WDT_CONFIG_REG         0x60008098u
#define RTC_WDT_PROTECT_REG        0x600080B0u
#define SUPER_WDT_CONFIG_REG       0x600080B4u
#define SUPER_WDT_PROTECT_REG      0x600080B8u
#define TIMER0_WDT_CONFIG_REG      0x6001F048u
#define TIMER0_WDT_PROTECT_REG     0x6001F064u

#define WDT_WRITE_KEY              0x50D83AA1u
#define SUPER_WDT_WRITE_KEY        0x8F1D312Au
#define WDT_ENABLE                 BIT(31)
#define RTC_WDT_FLASH_BOOT_ENABLE  BIT(12)
#define TIMER_WDT_FLASH_BOOT_ENABLE BIT(14)
#define SUPER_WDT_AUTO_FEED_ENABLE BIT(31)

#endif

