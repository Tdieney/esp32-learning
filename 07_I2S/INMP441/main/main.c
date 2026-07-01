/**
 * @file main.c
 * @brief Application entry point for the INMP441 audio recorder.
 *
 * Boot sequence:
 *  1. Mount LittleFS on the "storage" partition at /lfs
 *  2. Initialise the I2S RX channel for the INMP441 microphone
 *  3. Connect to Wi-Fi (Station mode)
 *  4. Start the HTTP web server
 *  5. Create the FreeRTOS recorder task
 *
 * HTTP endpoints (see http_server.c):
 *   GET /record   – capture 2 s of audio → /lfs/audio.wav
 *   GET /download – stream the WAV file to the client
 *   GET /delete   – remove the WAV file from Flash
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_littlefs.h"

#include "recorder.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "led_indicator.h"
#include "app_config.h"

/* --------------------------------------------------------------------------
 * Logging tag
 * -------------------------------------------------------------------------- */
static const char* TAG = "MAIN";

/* --------------------------------------------------------------------------
 * Shared FreeRTOS EventGroup (recorder ↔ HTTP handler synchronisation)
 * -------------------------------------------------------------------------- */
static EventGroupHandle_t s_rec_eg = NULL;

/* --------------------------------------------------------------------------
 * Helper: mount LittleFS
 * -------------------------------------------------------------------------- */
static esp_err_t littlefs_mount(void)
{
    ESP_LOGI(TAG, "Mounting LittleFS (partition=\"%s\", path=\"%s\")", LFS_PARTITION_LABEL, LFS_BASE_PATH);

    esp_vfs_littlefs_conf_t conf = {
        .base_path = LFS_BASE_PATH,
        .partition_label = LFS_PARTITION_LABEL,
        .format_if_mount_failed = true, /* Format on first boot */
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "LittleFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Log partition usage */
    size_t total = 0, used = 0;
    if (esp_littlefs_info(LFS_PARTITION_LABEL, &total, &used) == ESP_OK)
    {
        ESP_LOGI(
            TAG, "LittleFS mounted: %u KB used / %u KB total", (unsigned) (used / 1024), (unsigned) (total / 1024));
    }

    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * app_main
 * -------------------------------------------------------------------------- */
void app_main(void)
{
    ESP_LOGI(TAG, "=== INMP441 Audio Recorder booting ===");

    /* ---- 0. Initialise LED indicator (first, so boot status is visible) ---- */
    ESP_ERROR_CHECK(led_indicator_init());

    /* ---- 1. Mount LittleFS ---- */
    ESP_ERROR_CHECK(littlefs_mount());

    /* ---- 2. Initialise I2S RX channel ---- */
    ESP_ERROR_CHECK(recorder_i2s_init());

    /* ---- 3. Create the shared EventGroup ---- */
    s_rec_eg = xEventGroupCreate();
    if (s_rec_eg == NULL)
    {
        ESP_LOGE(TAG, "Failed to create recorder EventGroup");
        return;
    }

    /* ---- 4. Connect to Wi-Fi ---- */
    /* NOTE: Do NOT use ESP_ERROR_CHECK here – a failed Wi-Fi connection
     * would call abort() causing an infinite restart loop.
     * Instead log the error and halt cleanly so the user can diagnose. */
    esp_err_t wifi_ret = wifi_manager_init();
    if (wifi_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Wi-Fi failed (err=%s). Check WIFI_SSID/WIFI_PASS in app_config.h.", esp_err_to_name(wifi_ret));
        ESP_LOGE(TAG, "HTTP server will NOT start. Reset and fix credentials.");
        /* Blink LED red indefinitely to signal Wi-Fi error */
        while (1)
        {
            led_indicator_recording(); /* solid red on  */
            vTaskDelay(pdMS_TO_TICKS(200));
            led_indicator_idle(); /* off           */
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    /* ---- 5. Start HTTP server (passes EventGroup to handlers) ---- */
    ESP_ERROR_CHECK(http_server_start(s_rec_eg));

    /* ---- 6. Create the recorder FreeRTOS task ---- */
    BaseType_t rc = xTaskCreatePinnedToCore(recorder_task,    /* task function     */
                                            "recorder",       /* task name         */
                                            8192,             /* stack (bytes)     */
                                            (void*) s_rec_eg, /* parameter         */
                                            5,                /* priority          */
                                            NULL,             /* task handle (out) */
                                            1                 /* core ID: 1        */
    );
    if (rc != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create recorder task");
        return;
    }

    ESP_LOGI(TAG, "=== System ready. Open http://<device-ip>/record to start. ===");

    /* app_main can return – FreeRTOS scheduler continues running */
}
