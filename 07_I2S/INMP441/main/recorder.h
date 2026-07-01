/**
 * @file recorder.h
 * @brief Public API for the I2S recording module.
 *
 * Provides initialisation and the FreeRTOS task function that records
 * audio from the INMP441 microphone and writes a WAV file to LittleFS.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the I2S peripheral in standard RX mode.
 *
 * Must be called once from app_main before starting the recorder task.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t recorder_i2s_init(void);

/**
 * @brief FreeRTOS task function for audio recording.
 *
 * Waits for BIT_START_RECORDING on the shared EventGroup, records
 * RECORD_DURATION_SEC seconds of audio, writes the WAV file to LittleFS,
 * then sets BIT_RECORDING_DONE.  Loops indefinitely.
 *
 * @param pvParameters  Pointer to an EventGroupHandle_t (shared EventGroup).
 */
void recorder_task(void *pvParameters);

#ifdef __cplusplus
}
#endif
