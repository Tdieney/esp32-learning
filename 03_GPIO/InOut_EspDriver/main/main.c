#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char* TAG = "GPIO_BASIC";

/* CONFIG_MODE
 * 0: GPIO OUTPUT (Blink LED)
 * 1: GPIO INPUT (Button Polling)
 */
#define CONFIG_MODE 0

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
        vTaskDelay(pdMS_TO_TICKS(500)); // 500ms delay
    }
}

#elif (CONFIG_MODE == 1)
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

    // 2. Configure Button pin as Input with internal Pull-Up enabled
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Active-low configuration
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_conf);

    while (1)
    {
        // Read current logic level (0 = Pressed, 1 = Released)
        int btn_level = gpio_get_level(BUTTON_PIN);

        if (btn_level == 0)
        {
            gpio_set_level(LED_PIN, 1); // Turn ON LED when pressed
            ESP_LOGI(TAG, "Button PRESSED -> LED ON");
        }
        else
        {
            gpio_set_level(LED_PIN, 0); // Turn OFF LED when released
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms polling delay for debouncing
    }
}

#else
#error "Invalid CONFIG_MODE! Only 0 or 1 is accepted."
#endif
