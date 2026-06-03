#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DR_REG_GPIO_BASE      (0x60004000UL)
#define DR_REG_IO_MUX_BASE    (0x60009000UL)
#define GPIO_ENABLE_W1TS_REG  (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x24U))
#define GPIO_ENABLE1_W1TS_REG (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x30U))
#define GPIO_ENABLE_W1TC_REG  (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x28U))
#define GPIO_ENABLE1_W1TC_REG (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x34U))
#define GPIO_OUT_W1TS_REG     (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x08U))
#define GPIO_OUT1_W1TS_REG    (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x14U))
#define GPIO_OUT_W1TC_REG     (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x0CU))
#define GPIO_OUT1_W1TC_REG    (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x18U))

#define IO_MUX_GPIO_REG(n) (*(volatile uint32_t*) (DR_REG_IO_MUX_BASE + 0x4U + (n) * 4U))

#define REGISTER_WIDTH_BITS (32U)
#define BLINK_DELAY_MS      (1000U)

// Configure GPIO45 as output
void gpio_init_led(void)
{
    // IO_MUX_FUN_DRV = 10mA
    IO_MUX_GPIO_REG(45) &= ~(3UL << 10);
    IO_MUX_GPIO_REG(45) |= (1UL << 10);

    // Disable input
    IO_MUX_GPIO_REG(45) &= ~(1UL << 4);

    // Enable output
    GPIO_ENABLE1_W1TS_REG |= (1UL << (45 - REGISTER_WIDTH_BITS));
}

void gpio_register_set_high(uint32_t pin)
{
    if (pin < REGISTER_WIDTH_BITS)
    {
        GPIO_OUT_W1TS_REG |= (1UL << pin);
    }
    else
    {
        GPIO_OUT1_W1TS_REG |= (1UL << (pin - REGISTER_WIDTH_BITS));
    }
}

void gpio_register_set_low(uint32_t pin)
{
    if (pin < REGISTER_WIDTH_BITS)
    {
        GPIO_OUT_W1TC_REG |= (1UL << pin);
    }
    else
    {
        GPIO_OUT1_W1TC_REG |= (1UL << (pin - REGISTER_WIDTH_BITS));
    }
}

void app_main(void)
{
    gpio_init_led();

    while (1)
    {
        gpio_register_set_high(45);
        vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY_MS));
        gpio_register_set_low(45);
        vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY_MS));
    }
}
