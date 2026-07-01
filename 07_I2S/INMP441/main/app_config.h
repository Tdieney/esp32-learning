/**
 * @file app_config.h
 * @brief Centralized configuration for the INMP441 recorder project.
 *
 * All tuneable parameters (Wi-Fi credentials, pin mapping, audio settings,
 * file paths, FreeRTOS bits, LED indicator) are collected here so the
 * developer only needs to edit a single file before building.
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/* --------------------------------------------------------------------------
 * Wi-Fi credentials
 * -------------------------------------------------------------------------- */
#define WIFI_SSID "TranMen"
#define WIFI_PASS "0765336257"

/** Maximum number of reconnect attempts before giving up. */
#define WIFI_MAX_RETRY 5

/* --------------------------------------------------------------------------
 * I2S pin mapping  (INMP441 → ESP32-S3-DevKitC-1 v1.0)
 * --------------------------------------------------------------------------
 *  INMP441 pin   →   ESP32-S3 GPIO
 *  SCK           →   GPIO 41   (bit clock / BCLK)
 *  WS            →   GPIO 42   (word select / LRCLK)
 *  SD            →   GPIO  2   (serial data)
 *  L/R           →   GND       (selects left channel, address = 0)
 * -------------------------------------------------------------------------- */
#define I2S_BCLK_GPIO 41
#define I2S_WS_GPIO   42
#define I2S_DIN_GPIO  2 /* Data IN to ESP32 from mic */

/* --------------------------------------------------------------------------
 * Audio parameters
 * -------------------------------------------------------------------------- */
/** Sample rate in Hz. */
#define AUDIO_SAMPLE_RATE 16000

/**
 * The INMP441 outputs 32-bit I2S frames (24-bit audio, left-justified).
 * We read 32-bit words from DMA and down-shift to get 16-bit PCM.
 */
#define AUDIO_DMA_BITS I2S_DATA_BIT_WIDTH_32BIT

/** Number of bits in the final WAV file. */
#define AUDIO_WAV_BITS 16

/** Number of audio channels (1 = mono). */
#define AUDIO_CHANNELS 1

/**
 * Down-convert INMP441 32-bit DMA word → 16-bit PCM.
 *
 * Requires I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG (see recorder.c).
 * With Philips mode the ESP32 hardware inserts the 1-BCLK delay that
 * INMP441's Philips I2S format expects, so the 24-bit audio is placed at:
 *
 *   int32_t DMA word:
 *   bit 31 = D23  (MSB = sign bit of the 24-bit audio)
 *   bit 30 = D22
 *   ...
 *   bit  8 = D0   (LSB of the 24-bit audio)
 *   bit 7:0 = 0   (zero-padding from INMP441)
 *
 * Arithmetic right-shift by 16:
 *   → bit 15 = D23  (sign bit preserved ✓)
 *   → bit 14 = D22
 *   ...
 *   → bit  0 = D8
 *   → int16_t captures Audio[23:8]  — full dynamic range, no distortion.
 *
 * Common mistake: using I2S_STD_MSB_SLOT_DEFAULT_CONFIG shifts the whole
 * frame right by 1 (bit31 becomes blank=0, sign bit lands at bit30 instead
 * of bit31). Casting to int16 then misinterprets the sign → severe clipping
 * and distortion.
 */
#define AUDIO_SHIFT_BITS 16

/**
 * Software gain applied during 32-bit → 16-bit down-conversion.
 *
 * AUDIO_GAIN_SHIFT is the number of extra left-shift bits before the
 * final >> 16 truncation.  Each step doubles the amplitude (+6 dB):
 *
 *   0 → no gain  (default, captures Audio[23:8], correct full-range)
 *   1 → ×2  (+6 dB)
 *   2 → ×4  (+12 dB)
 *   3 → ×8  (+18 dB)
 *   4 → ×16 (+24 dB)
 *
 * Saturation clipping is applied automatically, so raising this value
 * too high only causes clipping (not wrap-around distortion).
 *
 * Formula: pcm = clamp(int32_dma >> (AUDIO_SHIFT_BITS - AUDIO_GAIN_SHIFT), -32768, 32767)
 */
#define AUDIO_GAIN_SHIFT 3

/**
 * Pre-roll duration (seconds) discarded before writing to the WAV file.
 * Removes the initial click / silence caused by microphone warm-up and
 * the user clicking the browser button.
 * The LED turns YELLOW during this period so you know to wait.
 */
#define RECORD_PREROLL_SEC 0.5f

/** Recording duration in seconds (not counting the pre-roll). */
#define RECORD_DURATION_SEC 2

/**
 * Total 16-bit PCM samples to capture (after pre-roll).
 *
 * Named RECORD_SAMPLES (not RECORD_WORDS) because in I2S_SLOT_MODE_MONO
 * the DMA delivers exactly 1 sample per 32-bit frame, so samples == words.
 * Keeping AUDIO_CHANNELS here is intentional: it documents the formula even
 * though AUDIO_CHANNELS is currently 1, so the macro stays correct if the
 * project is later changed to stereo.
 */
#define RECORD_SAMPLES ((uint32_t) (AUDIO_SAMPLE_RATE * RECORD_DURATION_SEC * AUDIO_CHANNELS))

/**
 * DMA read chunk size in bytes (reads of 32-bit samples).
 * 1024 samples × 4 bytes/sample = 4096 bytes per DMA read.
 */
#define I2S_READ_BUF_SAMPLES 1024
#define I2S_READ_BUF_BYTES   (I2S_READ_BUF_SAMPLES * sizeof(int32_t))

/* --------------------------------------------------------------------------
 * LittleFS / file system
 * -------------------------------------------------------------------------- */
/** Partition label must match the name column in partitions.csv. */
#define LFS_PARTITION_LABEL "storage"

/** VFS mount point for LittleFS. */
#define LFS_BASE_PATH "/lfs"

/** Full path of the WAV file inside the VFS (overwritten each recording). */
#define WAV_FILE_PATH LFS_BASE_PATH "/audio.wav"

/* --------------------------------------------------------------------------
 * Onboard WS2812 RGB LED  (ESP32-S3-DevKitC-1 v1.0 → GPIO 48)
 * -------------------------------------------------------------------------- */
/** GPIO connected to the WS2812 data-in pin. */
#define LED_GPIO 48

/** Number of WS2812 LEDs on the strip (DevKitC-1 has exactly 1). */
#define LED_NUM_LEDS 1

/**
 * Pre-roll yellow blink timing (half-period on = half-period off).
 * 3 blinks × 2 × LED_PREROLL_BLINK_MS = total yellow duration.
 */
#define LED_PREROLL_BLINK_MS    200 /* ms for each ON or OFF phase   */
#define LED_PREROLL_BLINK_COUNT 3   /* number of full on-off cycles  */

/** Duration (ms) the green "done" LED stays on after recording. */
#define LED_DONE_MS 500

/** Brightness 0-255 for each colour state. Keep below 80 to avoid
 *  drawing too much current from the USB 3.3 V rail. */
#define LED_BRIGHTNESS 60

/**
 * Total HTTP wait timeout (ms) for one /record request.
 *
 * Phases (sequential):
 *   Yellow LED blinks : LED_PREROLL_BLINK_COUNT × 2 × LED_PREROLL_BLINK_MS  = 1200 ms
 *   Pre-roll discard  : RECORD_PREROLL_SEC × 1000                            =  500 ms
 *   Actual recording  : RECORD_DURATION_SEC × 1000                           = 2000 ms
 *   Green LED done    : LED_DONE_MS                                           =  500 ms
 *   I/O overhead      : I2S scheduling + LittleFS write (measured ~1200 ms)  = 2000 ms
 *   Safety margin     :                                                       = 2000 ms
 *                                                              Total ≈ 8200 ms
 */
#define RECORD_TOTAL_TIMEOUT_MS                                                  \
    ((LED_PREROLL_BLINK_COUNT * 2 * LED_PREROLL_BLINK_MS)                        \
     + (uint32_t)(RECORD_PREROLL_SEC * 1000)                                     \
     + (RECORD_DURATION_SEC * 1000)                                              \
     + LED_DONE_MS                                                               \
     + 4000)



/* --------------------------------------------------------------------------
 * FreeRTOS synchronisation bits
 * -------------------------------------------------------------------------- */
#define BIT_START_RECORDING (1 << 0) /**< HTTP handler → recorder task */
#define BIT_RECORDING_DONE  (1 << 1) /**< Recorder task → HTTP handler  */

/* --------------------------------------------------------------------------
 * HTTP server
 * -------------------------------------------------------------------------- */
/** TCP port the web server listens on. */
#define HTTP_SERVER_PORT 80

#ifdef __cplusplus
}
#endif
