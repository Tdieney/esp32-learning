/**
 * @file wifi_manager.h
 * @brief Public API for Wi-Fi Station initialisation.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise Wi-Fi in Station mode and block until connected.
 *
 * Uses WIFI_SSID / WIFI_PASS macros from app_config.h.
 * Retries up to WIFI_MAX_RETRY times.  Logs the assigned IP on success.
 *
 * @return ESP_OK if connected, ESP_FAIL if max retries exceeded.
 */
esp_err_t wifi_manager_init(void);

#ifdef __cplusplus
}
#endif
