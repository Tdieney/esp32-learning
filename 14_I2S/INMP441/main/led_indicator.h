/**
 * @file led_indicator.h
 * @brief WS2812 RGB LED status indicator API.
 *
 * Uses the ESP-IDF built-in led_strip driver (RMT backend).
 * Target LED: onboard WS2812 on GPIO 48 of ESP32-S3-DevKitC-1 v1.0.
 *
 * Colour scheme:
 *   OFF    – idle / not recording
 *   YELLOW – pre-roll (0.5 s warm-up, discarding initial click)
 *   RED    – actively recording (blinks every LED_BLINK_PERIOD_MS ms)
 *   GREEN  – recording done, file saved (shown for ~1 s then off)
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the WS2812 LED strip driver.
 *
 * Must be called once from app_main before any other led_indicator_* call.
 *
 * @return ESP_OK on success.
 */
esp_err_t led_indicator_init(void);

/** @brief Turn the LED off (idle state). */
void led_indicator_idle(void);

/**
 * @brief Solid YELLOW – pre-roll / warm-up period.
 *
 * Call this just before starting the DMA discard loop.
 */
void led_indicator_preroll(void);

/**
 * @brief Solid RED – đang thu âm.
 *
 * Gọi đúng 1 lần ngay trước vòng lặp đọc I2S.
 * Đèn giữ nguyên cho đến khi gọi led_indicator_done().
 */
void led_indicator_recording(void);

/**
 * @brief Solid GREEN for ~1 second, then turn off.
 *
 * Blocks the calling task for 1000 ms.  Call from the recorder task
 * after the WAV file has been closed.
 */
void led_indicator_done(void);

#ifdef __cplusplus
}
#endif
