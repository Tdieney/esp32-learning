/**
 * @file wifi_manager.c
 * @brief Wi-Fi Station initialisation with retry logic.
 *
 * Connects to the AP specified by WIFI_SSID / WIFI_PASS in app_config.h.
 * Blocks until either an IP is obtained or WIFI_MAX_RETRY attempts have
 * been exhausted.
 */

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "wifi_manager.h"
#include "app_config.h"

/* --------------------------------------------------------------------------
 * Logging tag
 * -------------------------------------------------------------------------- */
static const char *TAG = "WIFI";

/* --------------------------------------------------------------------------
 * Internal synchronisation
 * -------------------------------------------------------------------------- */
#define WIFI_BIT_CONNECTED  BIT0
#define WIFI_BIT_FAILED     BIT1

static EventGroupHandle_t s_wifi_eg = NULL;
static int                s_retry   = 0;

/* --------------------------------------------------------------------------
 * Event handler
 * -------------------------------------------------------------------------- */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t          event_id,
                               void            *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* Guard: s_wifi_eg is deleted after initial connect; ignore
         * post-boot disconnects here (reconnect logic can be added later). */
        if (s_wifi_eg == NULL) {
            ESP_LOGW(TAG, "Wi-Fi disconnected after boot (not reconnecting)");
            return;
        }
        if (s_retry < WIFI_MAX_RETRY) {
            s_retry++;
            ESP_LOGW(TAG, "Connection lost – retry %d/%d", s_retry, WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            ESP_LOGE(TAG, "Max retries reached, giving up");
            xEventGroupSetBits(s_wifi_eg, WIFI_BIT_FAILED);
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        s_retry = 0;
        xEventGroupSetBits(s_wifi_eg, WIFI_BIT_CONNECTED);
    }
}

/* --------------------------------------------------------------------------
 * Public
 * -------------------------------------------------------------------------- */
esp_err_t wifi_manager_init(void)
{
    /* NVS is required by the Wi-Fi driver */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_eg = xEventGroupCreate();
    if (s_wifi_eg == NULL) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi EventGroup");
        return ESP_ERR_NO_MEM;
    }

    /* TCP/IP stack + default event loop */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register handlers for both WIFI_EVENT and IP_EVENT */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to SSID: %s …", WIFI_SSID);

    /* Block until connected or failed */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_eg,
                                           WIFI_BIT_CONNECTED | WIFI_BIT_FAILED,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_BIT_CONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi connected");
        vEventGroupDelete(s_wifi_eg);
        s_wifi_eg = NULL;
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Wi-Fi connection failed");
    vEventGroupDelete(s_wifi_eg);
    s_wifi_eg = NULL;
    return ESP_FAIL;
}
