typedef unsigned int uint32_t;

#define DR_REG_GPIO_BASE   (0x60004000UL)
#define DR_REG_IO_MUX_BASE (0x60009000UL)

#define GPIO_ENABLE_W1TS_REG  (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x24U))
#define GPIO_ENABLE1_W1TS_REG (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x30U))
#define GPIO_ENABLE_W1TC_REG  (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x28U))
#define GPIO_ENABLE1_W1TC_REG (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x34U))
#define GPIO_OUT_W1TS_REG     (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x08U))
#define GPIO_OUT1_W1TS_REG    (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x14U))
#define GPIO_OUT_W1TC_REG     (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x0CU))
#define GPIO_OUT1_W1TC_REG    (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x18U))

#define IO_MUX_GPIO_REG(n) (*(volatile uint32_t*) (DR_REG_IO_MUX_BASE + 0x4U + (n) * 4U))

// GPIO0: IO_MUX_GPIO0_REG offset = 0x4U
// GPIO1: IO_MUX_GPIO1_REG offset = 0x8U

#define REGISTER_WIDTH_BITS (32U)
#define BLINK_DELAY_MS      (1000U)

void delay_ms(uint32_t ms)
{
    while (ms--)
    {
        for (volatile uint32_t i = 0; i < 1000; i++)
        {
            // Do nothing
        }
    }
}

// Configure GPIO4 as output
void gpio_init_led(void)
{
    // IO_MUX_FUN_DRV = 10mA
    IO_MUX_GPIO_REG(4) &= ~(3UL << 10); // Clear bits 11:10
    IO_MUX_GPIO_REG(4) |= (1UL << 10);  // Set bit 10 to 1 (10mA drive strength)
    // Disable input
    IO_MUX_GPIO_REG(4) &= ~(1UL << 4);
    // Enable output (pin 4 < 32 -> use base ENABLE reg, not ENABLE1)
    GPIO_ENABLE_W1TS_REG = (1UL << 4);
}

void gpio_register_set_high(uint32_t pin)
{
    if (pin < REGISTER_WIDTH_BITS)
    {
        GPIO_OUT_W1TS_REG = (1UL << pin);
    }
    else
    {
        GPIO_OUT1_W1TS_REG = (1UL << (pin - REGISTER_WIDTH_BITS));
    }
}

void gpio_register_set_low(uint32_t pin)
{
    if (pin < REGISTER_WIDTH_BITS)
    {
        GPIO_OUT_W1TC_REG = (1UL << pin);
    }
    else
    {
        GPIO_OUT1_W1TC_REG = (1UL << (pin - REGISTER_WIDTH_BITS));
    }
}

void main(void)
{
    gpio_init_led();

    while (1)
    {
        gpio_register_set_high(4);
        delay_ms(BLINK_DELAY_MS);
        gpio_register_set_low(4);
        delay_ms(BLINK_DELAY_MS);
    }
}
