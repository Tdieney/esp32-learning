#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "led_strip.h"

/* ─── UART Configuration ─────────────────────────────────────────────────── */
#define UART_PORT_NUM   UART_NUM_1
#define UART_TX_PIN     GPIO_NUM_17
#define UART_RX_PIN     GPIO_NUM_18
#define UART_BUF_SIZE   1024U
#define UART_QUEUE_SIZE 10U

/* ─── RGB LED (WS2812 on GPIO 48) ────────────────────────────────────────── */
#define LED_STRIP_GPIO  48U

/* ─── Globals ────────────────────────────────────────────────────────────── */
static const char        *TAG = "[UART][ControlLED]";
static QueueHandle_t      uart_queue;
static led_strip_handle_t led_strip;

/* ─── Live UART config (mirrors the running driver state) ────────────────── */
static uart_config_t g_uart_cfg = {
    .baud_rate  = 115200,
    .data_bits  = UART_DATA_8_BITS,
    .parity     = UART_PARITY_DISABLE,
    .stop_bits  = UART_STOP_BITS_1,
    .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
};

/* ─── Pending config block (accumulated between START … END) ─────────────── */
typedef struct {
    bool     active;        /* true while inside START…END block    */
    bool     has_baud;
    bool     has_databits;
    bool     has_stopbits;
    bool     has_parity;
    bool     has_flowctrl;
    int      baud_rate;
    uart_word_length_t   data_bits;
    uart_stop_bits_t     stop_bits;
    uart_parity_t        parity;
    uart_hw_flowcontrol_t flow_ctrl;
} cfg_block_t;

static cfg_block_t g_cfg_block;

/* ─── LED helpers ────────────────────────────────────────────────────────── */
static void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

static void led_strip_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num   = LED_STRIP_GPIO,
        .max_leds         = 1U,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model        = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src       = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

/* ─── UART init / reconfigure ────────────────────────────────────────────── */
static void uart_init(void)
{
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM,
                                        UART_BUF_SIZE, UART_BUF_SIZE,
                                        UART_QUEUE_SIZE, &uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &g_uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM,
                                 UART_TX_PIN, UART_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "UART%d initialised - TX: GPIO%d  RX: GPIO%d  BaudRate: %d",
             UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, g_uart_cfg.baud_rate);
}

/**
 * @brief Apply a new uart_config_t to the running driver without
 *        uninstalling it (avoids losing the event-queue handle).
 */
static void uart_reconfigure(const uart_config_t *new_cfg)
{
    /* Flush any pending RX data before changing timing parameters */
    uart_flush(UART_PORT_NUM);

    esp_err_t err = uart_param_config(UART_PORT_NUM, new_cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        uart_write_bytes(UART_PORT_NUM, "ERROR: UART reconfigure failed\r\n", 32);
        return;
    }

    /* Mirror the new settings into the global shadow */
    g_uart_cfg = *new_cfg;

    char reply[128];
    int n = snprintf(reply, sizeof(reply),
                     "UART reconfigured: %d baud, %d data bits, %s stop, "
                     "parity=%s, flow=%s\r\n",
                     new_cfg->baud_rate,
                     (new_cfg->data_bits == UART_DATA_5_BITS) ? 5 :
                     (new_cfg->data_bits == UART_DATA_6_BITS) ? 6 :
                     (new_cfg->data_bits == UART_DATA_7_BITS) ? 7 : 8,
                     (new_cfg->stop_bits == UART_STOP_BITS_2)   ? "2"   :
                     (new_cfg->stop_bits == UART_STOP_BITS_1_5) ? "1.5" : "1",
                     (new_cfg->parity == UART_PARITY_EVEN) ? "EVEN" :
                     (new_cfg->parity == UART_PARITY_ODD)  ? "ODD"  : "NONE",
                     (new_cfg->flow_ctrl == UART_HW_FLOWCTRL_CTS_RTS) ? "CTS/RTS" :
                     (new_cfg->flow_ctrl == UART_HW_FLOWCTRL_CTS)     ? "CTS"     :
                     (new_cfg->flow_ctrl == UART_HW_FLOWCTRL_RTS)     ? "RTS"     : "NONE");
    uart_write_bytes(UART_PORT_NUM, reply, n);
    ESP_LOGI(TAG, "%.*s", n - 2, reply); /* strip trailing \r\n for log */
}

/* ─── Config-block state machine ─────────────────────────────────────────── */

/** Called when START is received – reset the pending block. */
static void cfg_block_start(void)
{
    memset(&g_cfg_block, 0, sizeof(g_cfg_block));
    g_cfg_block.active = true;

    /* Pre-fill with current running config as defaults */
    g_cfg_block.baud_rate  = g_uart_cfg.baud_rate;
    g_cfg_block.data_bits  = g_uart_cfg.data_bits;
    g_cfg_block.stop_bits  = g_uart_cfg.stop_bits;
    g_cfg_block.parity     = g_uart_cfg.parity;
    g_cfg_block.flow_ctrl  = g_uart_cfg.flow_ctrl;

    ESP_LOGI(TAG, "Config block started");
    uart_write_bytes(UART_PORT_NUM, "CONFIG BLOCK START\r\n", 20);
}

/** Parse a KEY=VALUE line while inside a config block. */
static void cfg_block_parse_line(const char *key, const char *val)
{
    if (strcmp(key, "BAUDRATE") == 0)
    {
        int baud = atoi(val);
        if (baud > 0)
        {
            g_cfg_block.baud_rate = baud;
            g_cfg_block.has_baud  = true;
            ESP_LOGI(TAG, "  BAUDRATE = %d", baud);
        }
        else
        {
            ESP_LOGW(TAG, "  Invalid BAUDRATE value: \"%s\"", val);
        }
    }
    else if (strcmp(key, "DATABITS") == 0)
    {
        int db = atoi(val);
        uart_word_length_t wl;
        switch (db)
        {
            case 5: wl = UART_DATA_5_BITS; break;
            case 6: wl = UART_DATA_6_BITS; break;
            case 7: wl = UART_DATA_7_BITS; break;
            case 8: wl = UART_DATA_8_BITS; break;
            default:
                ESP_LOGW(TAG, "  Invalid DATABITS value: \"%s\"", val);
                return;
        }
        g_cfg_block.data_bits    = wl;
        g_cfg_block.has_databits = true;
        ESP_LOGI(TAG, "  DATABITS = %d", db);
    }
    else if (strcmp(key, "STOPBITS") == 0)
    {
        uart_stop_bits_t sb;
        if      (strcmp(val, "1")   == 0) sb = UART_STOP_BITS_1;
        else if (strcmp(val, "1.5") == 0) sb = UART_STOP_BITS_1_5;
        else if (strcmp(val, "2")   == 0) sb = UART_STOP_BITS_2;
        else
        {
            ESP_LOGW(TAG, "  Invalid STOPBITS value: \"%s\"", val);
            return;
        }
        g_cfg_block.stop_bits    = sb;
        g_cfg_block.has_stopbits = true;
        ESP_LOGI(TAG, "  STOPBITS = %s", val);
    }
    else if (strcmp(key, "PARITY") == 0)
    {
        uart_parity_t p;
        if      (strcmp(val, "NONE") == 0) p = UART_PARITY_DISABLE;
        else if (strcmp(val, "EVEN") == 0) p = UART_PARITY_EVEN;
        else if (strcmp(val, "ODD")  == 0) p = UART_PARITY_ODD;
        else
        {
            ESP_LOGW(TAG, "  Invalid PARITY value: \"%s\"", val);
            return;
        }
        g_cfg_block.parity     = p;
        g_cfg_block.has_parity = true;
        ESP_LOGI(TAG, "  PARITY = %s", val);
    }
    else if (strcmp(key, "FLOWCONTROL") == 0)
    {
        uart_hw_flowcontrol_t fc;
        if      (strcmp(val, "NONE")    == 0) fc = UART_HW_FLOWCTRL_DISABLE;
        else if (strcmp(val, "RTS")     == 0) fc = UART_HW_FLOWCTRL_RTS;
        else if (strcmp(val, "CTS")     == 0) fc = UART_HW_FLOWCTRL_CTS;
        else if (strcmp(val, "CTS/RTS") == 0) fc = UART_HW_FLOWCTRL_CTS_RTS;
        else
        {
            ESP_LOGW(TAG, "  Invalid FLOWCONTROL value: \"%s\"", val);
            return;
        }
        g_cfg_block.flow_ctrl    = fc;
        g_cfg_block.has_flowctrl = true;
        ESP_LOGI(TAG, "  FLOWCONTROL = %s", val);
    }
    else
    {
        ESP_LOGW(TAG, "  Unknown config key: \"%s\"", key);
    }
}

/** Called when END is received – apply all collected parameters. */
static void cfg_block_end(void)
{
    if (!g_cfg_block.active)
    {
        ESP_LOGW(TAG, "END received without matching START – ignored");
        uart_write_bytes(UART_PORT_NUM, "ERROR: END without START\r\n", 26);
        return;
    }

    ESP_LOGI(TAG, "Config block ended – applying new UART settings");
    uart_write_bytes(UART_PORT_NUM, "CONFIG BLOCK END - Applying...\r\n", 32);

    uart_config_t new_cfg = {
        .baud_rate  = g_cfg_block.baud_rate,
        .data_bits  = g_cfg_block.data_bits,
        .stop_bits  = g_cfg_block.stop_bits,
        .parity     = g_cfg_block.parity,
        .flow_ctrl  = g_cfg_block.flow_ctrl,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_reconfigure(&new_cfg);

    memset(&g_cfg_block, 0, sizeof(g_cfg_block)); /* reset block state */
}

/* ─── Top-level command dispatcher ──────────────────────────────────────── */
/*
 * LED commands   : LED_ON | LED_OFF | RED | GREEN | BLUE
 * Config protocol: START … KEY=VALUE … END
 */
static void process_command(const char *cmd)
{
    ESP_LOGI(TAG, "Received command: \"%s\"", cmd);

    /* ── Config block boundary tokens ── */
    if (strcmp(cmd, "START") == 0)
    {
        cfg_block_start();
        return;
    }

    if (strcmp(cmd, "END") == 0)
    {
        cfg_block_end();
        return;
    }

    /* ── KEY=VALUE lines inside an active block ── */
    if (g_cfg_block.active)
    {
        /* Split on first '=' */
        char buf[64];
        strncpy(buf, cmd, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *eq = strchr(buf, '=');
        if (eq != NULL)
        {
            *eq = '\0';
            cfg_block_parse_line(buf, eq + 1);
        }
        else
        {
            ESP_LOGW(TAG, "  Line inside block has no '=': \"%s\"", cmd);
        }
        return;
    }

    /* ── LED control commands ── */
    if (strcmp(cmd, "LED_ON") == 0)
    {
        rgb_set(16, 16, 16);
        ESP_LOGI(TAG, "LED -> WHITE");
    }
    else if (strcmp(cmd, "LED_OFF") == 0)
    {
        led_strip_clear(led_strip);
        ESP_LOGI(TAG, "LED -> OFF");
    }
    else if (strcmp(cmd, "RED") == 0)
    {
        rgb_set(16, 0, 0);
        ESP_LOGI(TAG, "LED -> RED");
    }
    else if (strcmp(cmd, "GREEN") == 0)
    {
        rgb_set(0, 16, 0);
        ESP_LOGI(TAG, "LED -> GREEN");
    }
    else if (strcmp(cmd, "BLUE") == 0)
    {
        rgb_set(0, 0, 16);
        ESP_LOGI(TAG, "LED -> BLUE");
    }
    else
    {
        ESP_LOGW(TAG, "Unknown command: \"%s\"", cmd);
        uart_write_bytes(UART_PORT_NUM, "Unknown command\r\n", 17);
    }
}

/* ─── UART Event Task ────────────────────────────────────────────────────── */
static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t      data[UART_BUF_SIZE];
    char         cmd_buf[64];
    int          cmd_len = 0;

    ESP_LOGI(TAG, "UART event task started - waiting for commands...");
    uart_write_bytes(UART_PORT_NUM,
                     "ESP32 RGB LED Controller ready.\r\n"
                     "LED cmds : LED_ON | LED_OFF | RED | GREEN | BLUE\r\n"
                     "UART cfg : send config.txt (START...END block)\r\n",
                     131);

    while (1)
    {
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY) != pdTRUE)
            continue;

        switch (event.type)
        {
            case UART_DATA:
            {
                int len = uart_read_bytes(UART_PORT_NUM, data,
                                          event.size, pdMS_TO_TICKS(20));
                for (int i = 0; i < len; i++)
                {
                    char c = (char)data[i];
                    uart_write_bytes(UART_PORT_NUM, &c, 1); /* echo */

                    if (c == '\r' || c == '\n')
                    {
                        if (cmd_len > 0)
                        {
                            cmd_buf[cmd_len] = '\0';
                            uart_write_bytes(UART_PORT_NUM, "\r\n", 2);
                            process_command(cmd_buf);
                            cmd_len = 0;
                        }
                    }
                    else if (c == '\b' || c == 0x7F)
                    {
                        if (cmd_len > 0)
                        {
                            cmd_len--;
                            uart_write_bytes(UART_PORT_NUM, "\b \b", 3);
                        }
                    }
                    else if (cmd_len < (int)(sizeof(cmd_buf) - 1))
                    {
                        cmd_buf[cmd_len++] = c;
                    }
                }
                break;
            }

            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "UART FIFO overflow - flushing");
                uart_flush_input(UART_PORT_NUM);
                xQueueReset(uart_queue);
                cmd_len = 0;
                memset(&g_cfg_block, 0, sizeof(g_cfg_block));
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART ring-buffer full - flushing");
                uart_flush_input(UART_PORT_NUM);
                xQueueReset(uart_queue);
                cmd_len = 0;
                memset(&g_cfg_block, 0, sizeof(g_cfg_block));
                break;

            case UART_PARITY_ERR:
                ESP_LOGE(TAG, "UART parity error");
                break;

            case UART_FRAME_ERR:
                ESP_LOGE(TAG, "UART frame error");
                break;

            default:
                ESP_LOGD(TAG, "UART event type: %d", event.type);
                break;
        }
    }
}

/* ─── Entry Point ────────────────────────────────────────────────────────── */
void app_main(void)
{
    led_strip_init();
    uart_init();

    xTaskCreatePinnedToCore(uart_event_task, "uart_event_task",
                            4096, NULL, 12, NULL, 0);
}
