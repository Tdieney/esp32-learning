/**
 * @file led_indicator.c
 * @brief WS2812 RGB LED status indicator implementation.
 *
 * Uses the ESP-IDF v5.x built-in led_strip component (RMT backend).
 * One WS2812 LED on GPIO 48 of ESP32-S3-DevKitC-1 v1.0.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "led_strip.h"

#include "led_indicator.h"
#include "app_config.h"

/* --------------------------------------------------------------------------
 * Logging tag
 * -------------------------------------------------------------------------- */
static const char* TAG = "LED";

/* --------------------------------------------------------------------------
 * Module state
 * -------------------------------------------------------------------------- */
static led_strip_handle_t s_strip = NULL;

/* --------------------------------------------------------------------------
 * Internal helper: set pixel and refresh
 * -------------------------------------------------------------------------- */
static void set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    if (s_strip == NULL)
        return;

    /* Scale by LED_BRIGHTNESS (0-255) to avoid blinding / overcurrent */
    uint32_t br = LED_BRIGHTNESS;
    r = (uint8_t) ((r * br) / 255);
    g = (uint8_t) ((g * br) / 255);
    b = (uint8_t) ((b * br) / 255);

    led_strip_set_pixel(s_strip, 0, r, g, b);
    led_strip_refresh(s_strip);
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */
esp_err_t led_indicator_init(void)
{
    ESP_LOGI(TAG, "Initialising WS2812 on GPIO %d", LED_GPIO);

    led_strip_config_t strip_cfg = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_NUM_LEDS,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, /* WS2812 is GRB order */
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, /* 10 MHz – standard for WS2812 */
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Start with LED off */
    led_strip_clear(s_strip);
    ESP_LOGI(TAG, "WS2812 ready");
    return ESP_OK;
}

void led_indicator_idle(void)
{
    if (s_strip == NULL)
        return;
    led_strip_clear(s_strip); /* All pixels off */
}

void led_indicator_preroll(void)
{
    ESP_LOGI(TAG, "LED: YELLOW x3 blinks (pre-roll countdown – discarding click noise)");

    /*
     * Blink yellow 3 times to signal "recording is about to start".
     * During these ~900 ms the I2S DMA ring buffer keeps running and fills
     * with fresh data, naturally overwriting the old click-noise samples.
     * preroll_discard() then drains whatever remains in the buffer.
     *
     *  Blink 1 ──── Blink 2 ──── Blink 3
     *  [ON 150ms][OFF 150ms][ON 150ms][OFF 150ms][ON 150ms][OFF 150ms]
     *  |<─────────────────── ~900 ms ──────────────────────────────>|
     */
    for (int i = 0; i < LED_PREROLL_BLINK_COUNT; i++)
    {
        set_rgb(255, 180, 0);                        /* Yellow ON  */
        vTaskDelay(pdMS_TO_TICKS(LED_PREROLL_BLINK_MS));
        led_strip_clear(s_strip);                    /* Off        */
        vTaskDelay(pdMS_TO_TICKS(LED_PREROLL_BLINK_MS));
    }
}

void led_indicator_recording(void)
{
    /* Solid red – called once when recording begins */
    set_rgb(255, 0, 0);
    ESP_LOGI(TAG, "LED: RED solid (recording)");
}

void led_indicator_done(void)
{
    /* GREEN for LED_DONE_MS so the user has clear confirmation */
    set_rgb(0, 255, 0);
    ESP_LOGI(TAG, "LED: GREEN (recording done)");
    vTaskDelay(pdMS_TO_TICKS(LED_DONE_MS));
    led_indicator_idle();
}
