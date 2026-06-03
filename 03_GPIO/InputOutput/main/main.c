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
#define GPIO_IN_REG           (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x3CU))

#define IO_MUX_GPIO_REG(n) (*(volatile uint32_t*) (DR_REG_IO_MUX_BASE + 0x4U + (n) * 4U))

#define IO_MUX_FUN_WPU_POS (8U)
#define IO_MUX_FUN_IE_POS  (9U)
#define IO_MUX_FUN_DRV_POS (10U)
#define IO_MUX_MCU_SEL_POS (12U)

#define REGISTER_WIDTH_BITS (32U)
#define BTN_PIN             (4U)
#define LED_PIN             (45U)

// Configure GPIO4 as input pullup
void gpio_init_sw(void)
{
    // IO_MUX_FUN_WPU = 1
    IO_MUX_GPIO_REG(BTN_PIN) |= (1UL << IO_MUX_FUN_WPU_POS);

    // IO_MUX_MCU_SEL = 0
    IO_MUX_GPIO_REG(BTN_PIN) &= ~(7UL << IO_MUX_MCU_SEL_POS);

    // Disable output
    GPIO_ENABLE_W1TC_REG |= (1UL << BTN_PIN);

    // IO_MUX_FUN_IE = 1
    IO_MUX_GPIO_REG(BTN_PIN) |= (1UL << IO_MUX_FUN_IE_POS);
}

// Configure GPIO45 as output
void gpio_init_led(void)
{
    // IO_MUX_FUN_DRV = 10mA
    IO_MUX_GPIO_REG(LED_PIN) &= ~(3UL << IO_MUX_FUN_DRV_POS);
    IO_MUX_GPIO_REG(LED_PIN) |= (1UL << IO_MUX_FUN_DRV_POS);

    // Disable input
    IO_MUX_GPIO_REG(LED_PIN) &= ~(1UL << IO_MUX_FUN_IE_POS);

    // Enable output
    GPIO_ENABLE1_W1TS_REG |= (1UL << (LED_PIN - REGISTER_WIDTH_BITS));
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

uint32_t gpio_read(uint32_t pin)
{
    return ((GPIO_IN_REG & (1UL << pin)) != 0U);
}

void app_main(void)
{
    gpio_init_sw();
    gpio_init_led();

    while (1)
    {
        if (gpio_read(BTN_PIN) == 0U)
        {
            gpio_register_set_high(LED_PIN);
        }
        else
        {
            gpio_register_set_low(LED_PIN);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
