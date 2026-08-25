/**
 * @file http_server.h
 * @brief Public API for the HTTP web server module.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the HTTP server and register URI handlers.
 *
 * Registers:
 *   GET /record   – trigger recording
 *   GET /download – stream the WAV file
 *   GET /delete   – delete the WAV file
 *
 * @param eg  Shared FreeRTOS EventGroup used to synchronise with recorder_task.
 * @return    Handle to the running server, or NULL on failure.
 */
esp_err_t http_server_start(EventGroupHandle_t eg);

#ifdef __cplusplus
}
#endif
