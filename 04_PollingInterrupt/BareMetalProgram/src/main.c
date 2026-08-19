typedef unsigned int uint32_t;

/* CONFIG_MODE
 * 0: GPIO OUTPUT    (Blink LED on GPIO4)
 * 1: GPIO INPUT     (Poll the button on GPIO0, LED follows it)
 * 2: GPIO INTERRUPT (Button on GPIO0 raises an interrupt, LED toggles)
 */
#define CONFIG_MODE 2

#define DR_REG_GPIO_BASE      (0x60004000UL)
#define DR_REG_IO_MUX_BASE    (0x60009000UL)
#define DR_REG_INTERRUPT_BASE (0x600C2000UL)

#define GPIO_ENABLE_W1TS_REG  (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x24U))
#define GPIO_ENABLE1_W1TS_REG (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x30U))
#define GPIO_ENABLE_W1TC_REG  (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x28U))
#define GPIO_ENABLE1_W1TC_REG (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x34U))
#define GPIO_OUT_W1TS_REG     (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x08U))
#define GPIO_OUT1_W1TS_REG    (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x14U))
#define GPIO_OUT_W1TC_REG     (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x0CU))
#define GPIO_OUT1_W1TC_REG    (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x18U))
#define GPIO_IN_REG           (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x3CU))
#define GPIO_STATUS_REG       (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x44U))
#define GPIO_STATUS_W1TC_REG  (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x4CU))

// Per-pin interrupt configuration: GPIO_PIN0_REG sits at offset 0x74
#define GPIO_PIN_REG(n) (*(volatile uint32_t*) (DR_REG_GPIO_BASE + 0x74U + (n) * 4U))

// Interrupt matrix: which CPU line the "GPIO" peripheral source is routed to
#define INTERRUPT_CORE0_GPIO_MAP_REG (*(volatile uint32_t*) (DR_REG_INTERRUPT_BASE + 0x40U))

#define IO_MUX_GPIO_REG(n) (*(volatile uint32_t*) (DR_REG_IO_MUX_BASE + 0x4U + (n) * 4U))

// GPIO0: IO_MUX_GPIO0_REG offset = 0x4U
// GPIO1: IO_MUX_GPIO1_REG offset = 0x8U

// Bit positions inside IO_MUX_GPIOn_REG (ESP32-S3 TRM)
#define IO_MUX_FUN_WPD_POS (7U)  // Pull-down enable
#define IO_MUX_FUN_WPU_POS (8U)  // Pull-up enable
#define IO_MUX_FUN_IE_POS  (9U)  // Input buffer enable
#define IO_MUX_FUN_DRV_POS (10U) // Drive strength, bits 11:10
#define IO_MUX_MCU_SEL_POS (12U) // Pad function, bits 14:12
#define IO_MUX_FUNC_GPIO   (1U)  // MCU_SEL value that selects plain GPIO

// Bit positions inside GPIO_PINn_REG
#define GPIO_PIN_INT_TYPE_POS (7U)  // Edge/level selection, bits 9:7
#define GPIO_PIN_INT_ENA_POS  (13U) // Which CPU gets told, bits 17:13
#define GPIO_INT_TYPE_NEGEDGE (2U)  // Fire on the 1 -> 0 transition
#define GPIO_INT_ENA_CPU0     (1U)  // Bit 0 of that field = CPU0 interrupt

/* CPU interrupt 13: priority level 1, external, level triggered, and unused by
 * everything else here. Level 1 is the only priority the vector table in
 * startup.S knows how to service. */
#define CPU_INT_GPIO_NUM (13U)

#define REGISTER_WIDTH_BITS (32U)
#define LED_PIN             (4U)
#define BTN_PIN             (0U) // BOOT button on most ESP32-S3 boards (active low)
#define BLINK_DELAY_MS      (1000U)
#define DEBOUNCE_DELAY_MS   (50U)
#define SETTLE_DELAY_MS     (200U)

// Raised by the interrupt handler, consumed by main()
static volatile uint32_t btn_event;

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
    // MCU_SEL = 1 -> route the pad to the GPIO matrix instead of a peripheral
    IO_MUX_GPIO_REG(LED_PIN) &= ~(7UL << IO_MUX_MCU_SEL_POS);
    IO_MUX_GPIO_REG(LED_PIN) |= ((uint32_t) IO_MUX_FUNC_GPIO << IO_MUX_MCU_SEL_POS);

    // IO_MUX_FUN_DRV = 10mA
    IO_MUX_GPIO_REG(LED_PIN) &= ~(3UL << IO_MUX_FUN_DRV_POS); // Clear bits 11:10
    IO_MUX_GPIO_REG(LED_PIN) |= (1UL << IO_MUX_FUN_DRV_POS);  // 10mA drive strength

    // Disable input buffer, the pin is output only
    IO_MUX_GPIO_REG(LED_PIN) &= ~(1UL << IO_MUX_FUN_IE_POS);

    // Enable output (pin 4 < 32 -> use base ENABLE reg, not ENABLE1)
    GPIO_ENABLE_W1TS_REG = (1UL << LED_PIN);
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

/* Called from _exception_entry in startup.S once the interrupted state is
 * safely on the stack. Runs with every interrupt masked, so it stays short:
 * acknowledge the source, hand the event over to main(). */
void interrupt_dispatch(void)
{
    if ((GPIO_STATUS_REG & (1UL << BTN_PIN)) != 0U)
    {
        /* The CPU line is level triggered and the GPIO block holds it asserted
         * until this latched status bit is cleared. Skip the write and the
         * handler is re-entered forever. */
        GPIO_STATUS_W1TC_REG = (1UL << BTN_PIN);
        btn_event = 1U;
    }
}

#if (CONFIG_MODE >= 1)

// Configure GPIO0 as input with the internal pull-up enabled
void gpio_init_btn(void)
{
    // MCU_SEL = 1 -> plain GPIO function
    IO_MUX_GPIO_REG(BTN_PIN) &= ~(7UL << IO_MUX_MCU_SEL_POS);
    IO_MUX_GPIO_REG(BTN_PIN) |= ((uint32_t) IO_MUX_FUNC_GPIO << IO_MUX_MCU_SEL_POS);

    // Pull-up on, pull-down off -> the pad idles at 1 and drops to 0 when pressed
    IO_MUX_GPIO_REG(BTN_PIN) |= (1UL << IO_MUX_FUN_WPU_POS);
    IO_MUX_GPIO_REG(BTN_PIN) &= ~(1UL << IO_MUX_FUN_WPD_POS);

    // Enable the input buffer, otherwise GPIO_IN_REG always reads 0
    IO_MUX_GPIO_REG(BTN_PIN) |= (1UL << IO_MUX_FUN_IE_POS);

    // Disable output so the pin never fights the button
    GPIO_ENABLE_W1TC_REG = (1UL << BTN_PIN);
}

#endif

#if (CONFIG_MODE == 1)

// Read the logic level latched in GPIO_IN_REG (pins 0..31)
uint32_t gpio_read(uint32_t pin)
{
    return ((GPIO_IN_REG & (1UL << pin)) != 0U);
}

#endif

#if (CONFIG_MODE == 2)

// INTENABLE is a CPU special register, not a memory-mapped one
static void cpu_int_enable(uint32_t mask)
{
    __asm__ volatile ("wsr %0, intenable \n rsync" :: "r" (mask));
}

// Drop PS.INTLEVEL from 15 to 0 so level-1 interrupts are finally accepted
static void cpu_int_unmask(void)
{
    uint32_t old_ps;
    __asm__ volatile ("rsil %0, 0" : "=r" (old_ps));
    (void) old_ps;
}

// Make a press on GPIO0 reach the CPU as an interrupt
void gpio_init_btn_intr(void)
{
    // 1. Interrupt matrix: peripheral source "GPIO" -> CPU interrupt line 13
    INTERRUPT_CORE0_GPIO_MAP_REG = CPU_INT_GPIO_NUM;

    // 2. GPIO0 fires on the falling edge and reports to CPU0
    uint32_t pin_cfg = GPIO_PIN_REG(BTN_PIN);
    pin_cfg &= ~(7UL << GPIO_PIN_INT_TYPE_POS);
    pin_cfg |= ((uint32_t) GPIO_INT_TYPE_NEGEDGE << GPIO_PIN_INT_TYPE_POS);
    pin_cfg &= ~(0x1FUL << GPIO_PIN_INT_ENA_POS);
    pin_cfg |= ((uint32_t) GPIO_INT_ENA_CPU0 << GPIO_PIN_INT_ENA_POS);
    GPIO_PIN_REG(BTN_PIN) = pin_cfg;

    // 3. Throw away anything latched while we were configuring
    GPIO_STATUS_W1TC_REG = (1UL << BTN_PIN);

    // 4. Open the CPU line, then lower the global interrupt mask
    cpu_int_enable(1UL << CPU_INT_GPIO_NUM);
    cpu_int_unmask();
}

#endif

#if (CONFIG_MODE == 0)
void main(void)
{
    gpio_init_led();

    while (1)
    {
        gpio_register_set_high(LED_PIN);
        delay_ms(BLINK_DELAY_MS);
        gpio_register_set_low(LED_PIN);
        delay_ms(BLINK_DELAY_MS);
    }
}

#elif (CONFIG_MODE == 1)
void main(void)
{
    gpio_init_led();
    gpio_init_btn();

    while (1)
    {
        // Active low: 0 = pressed, 1 = released
        if (gpio_read(BTN_PIN) == 0U)
        {
            gpio_register_set_high(LED_PIN);
        }
        else
        {
            gpio_register_set_low(LED_PIN);
        }

        // Crude debounce: re-sample the pad only every ~50ms
        delay_ms(DEBOUNCE_DELAY_MS);
    }
}

#elif (CONFIG_MODE == 2)
void main(void)
{
    uint32_t led_state = 0U;

    gpio_init_led();
    gpio_init_btn();
    gpio_init_btn_intr();

    while (1)
    {
        // Nothing is polled here: the pad is only looked at when an edge arrives
        if (btn_event != 0U)
        {
            btn_event = 0U;

            led_state = !led_state;
            if (led_state != 0U)
            {
                gpio_register_set_high(LED_PIN);
            }
            else
            {
                gpio_register_set_low(LED_PIN);
            }

            // Let the contacts settle, then drop the edges they generated
            delay_ms(SETTLE_DELAY_MS);
            btn_event = 0U;
        }
    }
}

#else
#error "Invalid CONFIG_MODE! Only 0, 1 or 2 is accepted."
#endif
