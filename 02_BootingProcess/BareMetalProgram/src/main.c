#include <stdint.h>

#include "esp32s3_registers.h"

#define LED_GPIO 4u
#define LED_MASK BIT(LED_GPIO)

static void delay(void)
{
    /* A simple busy wait. The exact time is not important for this lesson. */
    for (volatile uint32_t count = 0; count < 2000000u; ++count) {
        __asm__ volatile("nop");
    }
}

static void led_init(void)
{
    uint32_t io_mux = REG32(IO_MUX_GPIO4_REG);

    io_mux &= ~IO_MUX_FUNCTION_MASK;
    io_mux |= IO_MUX_FUNCTION_GPIO;
    REG32(IO_MUX_GPIO4_REG) = io_mux;

    REG32(GPIO_FUNC4_OUT_SEL_REG) = GPIO_MATRIX_SIMPLE_OUTPUT;
    REG32(GPIO_ENABLE_W1TS_REG) = LED_MASK;
}

void __attribute__((noreturn)) main(void)
{
    led_init();

    for (;;) {
        REG32(GPIO_OUT_W1TS_REG) = LED_MASK;
        delay();

        REG32(GPIO_OUT_W1TC_REG) = LED_MASK;
        delay();
    }
}

