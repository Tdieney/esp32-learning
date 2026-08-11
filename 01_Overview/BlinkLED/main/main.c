#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "led_strip.h"
#include "esp_err.h"
#include "esp_log.h"

#define RGB_GPIO   48
#define BLINK_GPIO 47

static const char* TAG = "RGB LED";

void app_main(void)
{
    ESP_LOGI(TAG, "Start");

    led_strip_handle_t strip = NULL;

    led_strip_config_t strip_config = {
        .strip_gpio_num = RGB_GPIO,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));

    while (1)
    {
        ESP_LOGI(TAG, "RED");
        ESP_ERROR_CHECK(led_strip_set_pixel(strip, 0, 10, 0, 0));
        ESP_ERROR_CHECK(led_strip_refresh(strip));
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "GREEN");
        ESP_ERROR_CHECK(led_strip_set_pixel(strip, 0, 0, 10, 0));
        ESP_ERROR_CHECK(led_strip_refresh(strip));
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "BLUE");
        ESP_ERROR_CHECK(led_strip_set_pixel(strip, 0, 0, 0, 10));
        ESP_ERROR_CHECK(led_strip_refresh(strip));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
