#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char* TAG = "GPIO_BASIC";

/* CONFIG_MODE
 * 0: GPIO OUTPUT (Blink LED)
 * 1: GPIO INPUT (Button Interrupt with Flag)
 */
#define CONFIG_MODE 1

// GPIO Pin Definitions
#define LED_PIN    GPIO_NUM_4
#define BUTTON_PIN GPIO_NUM_0

#if (CONFIG_MODE == 0)
void app_main(void)
{
    // Configure GPIO for LED output
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    int led_state = 0;
    while (1)
    {
        // Toggle state and write logic level to the pin
        gpio_set_level(LED_PIN, led_state);
        ESP_LOGI(TAG, "LED State: %s", led_state ? "ON (1)" : "OFF (0)");

        led_state = !led_state;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

#elif (CONFIG_MODE == 1)

// Volatile flag to signal that an interrupt occurred
static volatile bool btn_pressed_flag = false;

/* Interrupt Service Routine (ISR).
 * Runs in interrupt context: keep it as short as possible.
 * It only sets the flag to notify the main loop, then returns immediately.
 * No blocking calls, delays, or logging (ESP_LOG/printf) should be used here.
 */
static void IRAM_ATTR button_isr_handler(void* arg)
{
    btn_pressed_flag = true;
}

void app_main(void)
{
    // 1. Configure LED pin as Output
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED_PIN, 0);

    // 2. Configure Button pin as Input with internal Pull-Up enabled, interrupt on falling edge
    //    (Active-low button: logic level transitions 1 -> 0 when pressed)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn_conf);

    // 3. Install the GPIO ISR service and attach our ISR handler to the button pin
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL));

    ESP_LOGI(TAG, "Waiting for button interrupt on GPIO%d...", BUTTON_PIN);

    int led_state = 0;

    // 4. Main loop checks the flag set by the ISR
    while (1)
    {
        int a = 0;
        if (btn_pressed_flag)
        {
            btn_pressed_flag = false; // Clear the flag

            // Software debounce: wait 50ms and verify button is still pressed
            vTaskDelay(pdMS_TO_TICKS(50));
            if (gpio_get_level(BUTTON_PIN) == 0)
            {
                led_state = !led_state;
                gpio_set_level(LED_PIN, led_state);
                ESP_LOGI(TAG, "Button pressed -> LED State: %s", led_state ? "ON (1)" : "OFF (0)");
            }
        }

        // Short delay to yield CPU and prevent watchdog timer timeout
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

#else
#error "Invalid CONFIG_MODE! Only 0 or 1 is accepted."
#endif
