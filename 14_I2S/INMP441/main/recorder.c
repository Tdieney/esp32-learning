/**
 * @file recorder.c
 * @brief I2S audio recording module for INMP441 microphone.
 *
 * Uses the modern ESP-IDF v5.x I2S standard driver (i2s_std.h).
 *
 * Recording sequence per trigger:
 *  1. LED → YELLOW  : pre-roll (RECORD_PREROLL_SEC), DMA data discarded.
 *                     Eliminates the initial click / silence from the user
 *                     clicking the browser button.
 *  2. LED → RED blink: actual recording (RECORD_DURATION_SEC seconds).
 *                     32-bit DMA words shifted to 16-bit PCM, written to WAV.
 *  3. LED → GREEN   : file closed, 1-second confirmation flash.
 *  4. LED → OFF     : idle, waiting for next trigger.
 *
 * Synchronisation with the HTTP handler is via a FreeRTOS EventGroup:
 *   BIT_START_RECORDING  – set by HTTP /record handler to start a recording
 *   BIT_RECORDING_DONE   – set by this task when the WAV file is ready
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

#include "driver/i2s_std.h"
#include "driver/gpio.h"

#include "recorder.h"
#include "led_indicator.h"
#include "app_config.h"

/* --------------------------------------------------------------------------
 * Logging tag
 * -------------------------------------------------------------------------- */
static const char *TAG = "RECORDER";

/* --------------------------------------------------------------------------
 * WAV header – 44 bytes, little-endian, packed struct
 * -------------------------------------------------------------------------- */
typedef struct {
    /* ---- RIFF chunk ---- */
    uint8_t  riff_id[4];        /**< "RIFF"                                */
    uint32_t riff_size;         /**< File size minus 8 bytes               */
    uint8_t  wave_id[4];        /**< "WAVE"                                */
    /* ---- fmt sub-chunk ---- */
    uint8_t  fmt_id[4];         /**< "fmt "                                */
    uint32_t fmt_size;          /**< Size of fmt sub-chunk = 16            */
    uint16_t audio_format;      /**< PCM = 1                               */
    uint16_t num_channels;      /**< 1 = mono                              */
    uint32_t sample_rate;       /**< e.g. 16000                            */
    uint32_t byte_rate;         /**< sample_rate × num_channels × bits/8   */
    uint16_t block_align;       /**< num_channels × bits/8                 */
    uint16_t bits_per_sample;   /**< 16                                    */
    /* ---- data sub-chunk ---- */
    uint8_t  data_id[4];        /**< "data"                                */
    uint32_t data_size;         /**< Number of audio data bytes            */
} __attribute__((packed)) wav_header_t;

/* Compile-time sanity check */
_Static_assert(sizeof(wav_header_t) == 44, "WAV header must be exactly 44 bytes");

/* --------------------------------------------------------------------------
 * Module-level I2S channel handle (initialised by recorder_i2s_init)
 * -------------------------------------------------------------------------- */
static i2s_chan_handle_t s_rx_handle = NULL;

/* --------------------------------------------------------------------------
 * Derived constants
 * --------------------------------------------------------------------------
 * Pre-roll: number of 32-bit DMA words to read and discard.
 * Use uint32_t to avoid floating-point issues at link time on Xtensa.
 * -------------------------------------------------------------------------- */

/** Total DMA words (int32_t) to discard during pre-roll. */
#define PREROLL_WORDS   ((uint32_t)(AUDIO_SAMPLE_RATE * RECORD_PREROLL_SEC))

/*
 * RECORD_SAMPLES is defined in app_config.h.
 * It equals AUDIO_SAMPLE_RATE * RECORD_DURATION_SEC * AUDIO_CHANNELS.
 */

/* --------------------------------------------------------------------------
 * Helper: build a valid WAV header for a mono 16-bit PCM file
 * -------------------------------------------------------------------------- */
static void wav_header_fill(wav_header_t *hdr, uint32_t num_samples)
{
    const uint32_t data_size   = num_samples * AUDIO_CHANNELS * (AUDIO_WAV_BITS / 8);
    const uint32_t byte_rate   = AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * (AUDIO_WAV_BITS / 8);
    const uint16_t block_align = (uint16_t)(AUDIO_CHANNELS * (AUDIO_WAV_BITS / 8));

    memcpy(hdr->riff_id,  "RIFF", 4);
    hdr->riff_size       = data_size + sizeof(wav_header_t) - 8;
    memcpy(hdr->wave_id,  "WAVE", 4);
    memcpy(hdr->fmt_id,   "fmt ", 4);
    hdr->fmt_size        = 16;
    hdr->audio_format    = 1;               /* PCM */
    hdr->num_channels    = AUDIO_CHANNELS;
    hdr->sample_rate     = AUDIO_SAMPLE_RATE;
    hdr->byte_rate       = byte_rate;
    hdr->block_align     = block_align;
    hdr->bits_per_sample = AUDIO_WAV_BITS;
    memcpy(hdr->data_id,  "data", 4);
    hdr->data_size       = data_size;
}

/* --------------------------------------------------------------------------
 * Public: initialise I2S RX channel
 * -------------------------------------------------------------------------- */
esp_err_t recorder_i2s_init(void)
{
    ESP_LOGI(TAG, "Initialising I2S RX channel");

    /* Channel configuration: RX only, port 0 */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    ESP_RETURN_ON_ERROR(
        i2s_new_channel(&chan_cfg, NULL, &s_rx_handle),
        TAG, "i2s_new_channel failed"
    );

    /*
     * Standard I2S configuration for INMP441:
     *
     * INMP441 uses Philips I2S format: MSB is output exactly ONE BCLK
     * after the WS edge (not immediately).  We must use
     * I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG so the ESP32 hardware
     * inserts the matching 1-cycle delay on RX.
     *
     * With Philips mode the DMA 32-bit word layout is:
     *   bit 31 = D23  (MSB / sign bit of 24-bit audio)
     *   bit 30 = D22
     *   ....
     *   bit  8 = D0   (LSB of 24-bit audio)
     *   bit 7:0 = 0   (zero-padding)
     *
     * Down-converting to 16-bit: arithmetic >> 16 places D23 at
     * bit 15 (int16 sign bit) and D22:D8 at bits 14:0. Perfect.
     *
     * Using I2S_STD_MSB_SLOT_DEFAULT_CONFIG instead would shift
     * everything right by 1 bit (bit31 becomes a blank '0', D23
     * lands at bit30) so the sign bit is wrong → severe distortion.
     *
     *  - 32-bit DMA slot width  (mic outputs 24-bit in a 32-bit frame)
     *  - Mono (only left channel because L/R pin is tied to GND)
     */
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        AUDIO_DMA_BITS,         /* I2S_DATA_BIT_WIDTH_32BIT */
                        I2S_SLOT_MODE_MONO      /* single microphone        */
                    ),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_GPIO,
            .ws   = I2S_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_DIN_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    /* Select only the left (channel 0) slot for a mono mic */
    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    ESP_RETURN_ON_ERROR(
        i2s_channel_init_std_mode(s_rx_handle, &std_cfg),
        TAG, "i2s_channel_init_std_mode failed"
    );

    ESP_RETURN_ON_ERROR(
        i2s_channel_enable(s_rx_handle),
        TAG, "i2s_channel_enable failed"
    );

    ESP_LOGI(TAG, "I2S RX ready: %d Hz, 32-bit DMA, mono "
             "BCLK=GPIO%d WS=GPIO%d DIN=GPIO%d",
             AUDIO_SAMPLE_RATE, I2S_BCLK_GPIO, I2S_WS_GPIO, I2S_DIN_GPIO);

    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * Internal: drain I2S DMA for 'num_words' 32-bit words (pre-roll discard)
 * -------------------------------------------------------------------------- */
static void preroll_discard(int32_t *dma_buf)
{
    uint32_t discarded = 0;

    /*
     * Safety cap: at most PREROLL_WORDS × 2 read attempts.
     * Each attempt reads up to I2S_READ_BUF_SAMPLES words, so we can
     * finish PREROLL_WORDS samples in as little as 1 iteration.
     * The cap prevents an infinite loop if the I2S peripheral stalls.
     */
    const uint32_t max_iters = (PREROLL_WORDS / I2S_READ_BUF_SAMPLES + 1) * 2;
    uint32_t iters = 0;

    while (discarded < PREROLL_WORDS && iters < max_iters) {
        size_t bytes_read = 0;
        i2s_channel_read(s_rx_handle,
                         dma_buf,
                         I2S_READ_BUF_BYTES,
                         &bytes_read,
                         pdMS_TO_TICKS(200));

        uint32_t words = (uint32_t)(bytes_read / sizeof(int32_t));
        uint32_t remaining = PREROLL_WORDS - discarded;
        if (words > remaining) words = remaining;
        discarded += words;
        iters++;
    }

    if (iters >= max_iters && discarded < PREROLL_WORDS) {
        ESP_LOGW(TAG, "Pre-roll: hit max iterations (%u), only discarded %u/%u samples",
                 (unsigned)max_iters, (unsigned)discarded, (unsigned)PREROLL_WORDS);
    } else {
        ESP_LOGI(TAG, "Pre-roll done: discarded %u samples (%.2f s)",
                 (unsigned)discarded,
                 (double)discarded / AUDIO_SAMPLE_RATE);
    }
}

/* --------------------------------------------------------------------------
 * Public: FreeRTOS recorder task
 * -------------------------------------------------------------------------- */
void recorder_task(void *pvParameters)
{
    EventGroupHandle_t eg = (EventGroupHandle_t)pvParameters;

    /* DMA read buffer: holds I2S_READ_BUF_SAMPLES × 32-bit words */
    int32_t *dma_buf = (int32_t *)malloc(I2S_READ_BUF_BYTES);
    if (dma_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer (%u bytes)",
                 (unsigned)I2S_READ_BUF_BYTES);
        vTaskDelete(NULL);
        return;
    }

    /* 16-bit PCM output buffer */
    int16_t *pcm_buf = (int16_t *)malloc(I2S_READ_BUF_SAMPLES * sizeof(int16_t));
    if (pcm_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate PCM buffer");
        free(dma_buf);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Recorder task running – waiting for /record trigger…");
    led_indicator_idle();

    while (1) {
        /* ----------------------------------------------------------------
         * Wait for the HTTP /record handler to signal us
         * ---------------------------------------------------------------- */
        xEventGroupWaitBits(eg,
                            BIT_START_RECORDING,
                            pdTRUE,   /* clear on exit */
                            pdFALSE,
                            portMAX_DELAY);

        /* ================================================================
         * PHASE 1 – Pre-roll: yellow LED, discard click / silence
         * ================================================================ */
        ESP_LOGI(TAG, ">>> PRE-ROLL: %.1f s", (double)RECORD_PREROLL_SEC);
        led_indicator_preroll();
        preroll_discard(dma_buf);

        /* ================================================================
         * PHASE 2 – Recording: red LED, write WAV to LittleFS
         * ================================================================ */
        ESP_LOGI(TAG, ">>> RECORDING: %d s at %d Hz mono 16-bit",
                 RECORD_DURATION_SEC, AUDIO_SAMPLE_RATE);

        FILE *fp = fopen(WAV_FILE_PATH, "wb");
        if (fp == NULL) {
            ESP_LOGE(TAG, "Cannot open %s for writing", WAV_FILE_PATH);
            led_indicator_idle();
            xEventGroupSetBits(eg, BIT_RECORDING_DONE);
            continue;
        }

        /* Reserve space for the WAV header */
        wav_header_t hdr;
        memset(&hdr, 0, sizeof(hdr));
        fwrite(&hdr, sizeof(hdr), 1, fp);

        uint32_t samples_written = 0;
        led_indicator_recording();

        while (samples_written < RECORD_SAMPLES) {
            size_t bytes_read = 0;
            esp_err_t ret = i2s_channel_read(s_rx_handle,
                                             dma_buf,
                                             I2S_READ_BUF_BYTES,
                                             &bytes_read,
                                             pdMS_TO_TICKS(200));
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "i2s_channel_read error: %s", esp_err_to_name(ret));
                break;
            }

            size_t   words_read = bytes_read / sizeof(int32_t);
            uint32_t remaining  = RECORD_SAMPLES - samples_written;
            if (words_read > remaining) words_read = remaining;

            /*
             * Down-convert 32-bit → 16-bit with software gain.
             * Philips mode: bit31 = D23 (sign bit), bits [31:8] = 24-bit audio.
             * net_shift = 16 - AUDIO_GAIN_SHIFT  (13 for gain=3, i.e. ×8 = +18 dB)
             * Saturate before cast to prevent wrap-around on loud signals.
             */
            const int net_shift = AUDIO_SHIFT_BITS - AUDIO_GAIN_SHIFT;
            for (size_t i = 0; i < words_read; i++) {
                int32_t s = dma_buf[i] >> net_shift;
                if      (s >  32767) s =  32767;
                else if (s < -32768) s = -32768;
                pcm_buf[i] = (int16_t)s;
            }

            size_t pcm_bytes = words_read * sizeof(int16_t);
            size_t written   = fwrite(pcm_buf, 1, pcm_bytes, fp);
            if (written != pcm_bytes) {
                ESP_LOGE(TAG, "File write error (%u/%u bytes)",
                         (unsigned)written, (unsigned)pcm_bytes);
                break;
            }

            samples_written += (uint32_t)words_read;
        }

        /* Write the completed WAV header */
        wav_header_fill(&hdr, samples_written);
        rewind(fp);
        fwrite(&hdr, sizeof(hdr), 1, fp);
        fclose(fp);

        /* ================================================================
         * PHASE 3 – Done: green LED flash, then signal HTTP handler
         * ================================================================ */
        ESP_LOGI(TAG, ">>> DONE: %u samples → %s",
                 (unsigned)samples_written, WAV_FILE_PATH);
        led_indicator_done();

        xEventGroupSetBits(eg, BIT_RECORDING_DONE);
        ESP_LOGI(TAG, "Waiting for next /record trigger…");
    }

    /* Should never reach here */
    free(dma_buf);
    free(pcm_buf);
    vTaskDelete(NULL);
}

