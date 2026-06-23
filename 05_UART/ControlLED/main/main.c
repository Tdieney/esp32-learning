#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "led_strip.h"

#define UART_PORT_NUM   UART_NUM_1
#define UART_TX_PIN     GPIO_NUM_17
#define UART_RX_PIN     GPIO_NUM_18
#define UART_BAUD_RATE  115200UL
#define UART_BUF_SIZE   1024U
#define UART_QUEUE_SIZE 10U
#define LED_STRIP_GPIO  48U

static const char* TAG = "[UART][ControlLED]";
static QueueHandle_t uart_queue;
static led_strip_handle_t led_strip;

static void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(led_strip, 0, r, g, b);
    led_strip_refresh(led_strip);
}

static void led_strip_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = 1U,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, /* WS2812 is GRB */
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, /* 10 MHz */
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    /* Start with LED off */
    led_strip_clear(led_strip);
}

static void uart_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    /* Install driver with event queue (interrupt-driven RX) */
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE, UART_BUF_SIZE, UART_QUEUE_SIZE, &uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG,
             "UART%d initialised - TX: GPIO%d  RX: GPIO%d  BaudRate: %lu",
             UART_PORT_NUM,
             UART_TX_PIN,
             UART_RX_PIN,
             UART_BAUD_RATE);
}

/*
 * "LED_ON": turn on LED (white)
 * "LED_OFF": turn off LED
 * "RED": change LED color to red
 * "GREEN": change LED color to green
 * "BLUE": change LED color to blue
 */
static void process_command(const char* cmd)
{
    ESP_LOGI(TAG, "Received command: \"%s\"", cmd);

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
    }
}

static void uart_event_task(void* pvParameters)
{
    uart_event_t event;
    uint8_t data[UART_BUF_SIZE];
    char cmd_buf[64];
    int cmd_len = 0;

    ESP_LOGI(TAG, "UART event task started - waiting for commands...");

    while (1)
    {
        /* Block until the UART ISR posts an event */
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY) != pdTRUE)
            continue;

        switch (event.type)
        {
            case UART_DATA:
            {
                int len = uart_read_bytes(UART_PORT_NUM, data, event.size, pdMS_TO_TICKS(20));
                for (int i = 0; i < len; i++)
                {
                    char c = (char) data[i];

                    /* Echo character back to terminal */
                    uart_write_bytes(UART_PORT_NUM, &c, 1);

                    if (c == '\r' || c == '\n')
                    {
                        if (cmd_len > 0)
                        {
                            cmd_buf[cmd_len] = '\0';
                            uart_write_bytes(UART_PORT_NUM, "\n", 2);
                            process_command(cmd_buf);
                            cmd_len = 0;
                        }
                    }
                    /* Backspace / DEL */
                    else if (c == '\b' || c == 0x7F)
                    {
                        if (cmd_len > 0)
                        {
                            cmd_len--;
                            uart_write_bytes(UART_PORT_NUM, "\b \b", 3);
                        }
                    }
                    else if (cmd_len < (int) (sizeof(cmd_buf) - 1))
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
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART ring-buffer full - flushing");
                uart_flush_input(UART_PORT_NUM);
                xQueueReset(uart_queue);
                cmd_len = 0;
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

void app_main(void)
{
    led_strip_init();
    uart_init();

    /* UART event task: Core 0, 4 kB stack, priority 12 */
    xTaskCreatePinnedToCore(uart_event_task, "uart_event_task", 4096, NULL, 12, NULL, 0);
}
