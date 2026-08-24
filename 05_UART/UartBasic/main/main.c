#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"

/*
 * =========================================================================
 * UART_MODE Selection:
 *   0: POLLING MODE   - Periodically poll UART RX buffer with timeout in app_main
 *   1: INTERRUPT MODE - Event queue driven by UART hardware ISR
 * =========================================================================
 */
#define UART_MODE 0

#define UART_PORT_NUM  UART_NUM_1
#define UART_TX_PIN    GPIO_NUM_17
#define UART_RX_PIN    GPIO_NUM_18
#define UART_BAUD_RATE 115200UL
#define UART_BUF_SIZE  128U

static const char* TAG = "[UART]";

/*
 * Allocate buffers in BSS (static memory) rather than on the task stack
 * to prevent stack overflow in task main.
 */
static uint8_t rx_data[UART_BUF_SIZE];
static char tx_buffer[UART_BUF_SIZE + 32];

#if (UART_MODE == 0)
/* =========================================================================
 * MODE 0: POLLING MODE
 * - Driver installed without an event queue (queue_size = 0, queue = NULL).
 * - CPU periodically polls incoming bytes with a timeout via uart_read_bytes().
 * ========================================================================= */

static void uart_polling_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    /* Install driver with RX ring buffer, no TX ring buffer, no event queue */
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void app_main(void)
{
    // 1. Initialize UART peripheral in Polling Mode
    uart_polling_init();

    // 2. Main polling loop
    while (1)
    {
        /*
         * Block up to 100ms waiting for incoming bytes (timeout-based polling).
         * Returns number of bytes read, or 0 if timeout expired without data.
         */
        int len = uart_read_bytes(UART_PORT_NUM, rx_data, sizeof(rx_data) - 1, pdMS_TO_TICKS(100));

        if (len > 0)
        {
            rx_data[len] = '\0'; // Null-terminate string
            ESP_LOGI(TAG, "[Polling RX] Received (%d bytes): %s", len, (char*) rx_data);

            // Echo received data back over UART
            int echo_len = snprintf(tx_buffer, sizeof(tx_buffer), "\r\n[Echo - Polling]: %s\r\n", (char*) rx_data);
            uart_write_bytes(UART_PORT_NUM, tx_buffer, echo_len);
        }
    }
}

#elif (UART_MODE == 1)
/* =========================================================================
 * MODE 1: INTERRUPT / EVENT QUEUE MODE
 * - Driver installed WITH a FreeRTOS event queue.
 * - Hardware UART ISR pushes events into the queue when data arrives.
 * - app_main blocks on xQueueReceive (0% CPU usage) until an interrupt occurs.
 * ========================================================================= */

#define UART_QUEUE_SIZE 10U
static QueueHandle_t uart_queue;

static void uart_interrupt_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    /* Install driver with FreeRTOS event queue for ISR-driven reception */
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, UART_QUEUE_SIZE, &uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void app_main(void)
{
    // 1. Initialize UART peripheral in Interrupt Mode
    uart_interrupt_init();

    uart_event_t event;

    // 2. Event loop driven by UART ISR
    while (1)
    {
        /*
         * Block indefinitely (portMAX_DELAY) until UART ISR posts an event into uart_queue.
         * Consumes 0% CPU while waiting.
         */
        if (xQueueReceive(uart_queue, &event, portMAX_DELAY) == pdTRUE)
        {
            switch (event.type)
            {
                case UART_DATA:
                {
                    int len = uart_read_bytes(UART_PORT_NUM, rx_data, event.size, pdMS_TO_TICKS(20));
                    if (len > 0)
                    {
                        rx_data[len] = '\0';
                        ESP_LOGI(TAG,
                                 "[Interrupt RX] Event size: %d, Read: %d bytes -> %s",
                                 (int) event.size,
                                 len,
                                 (char*) rx_data);

                        // Echo received data back over UART
                        int echo_len =
                            snprintf(tx_buffer, sizeof(tx_buffer), "\r\n[Echo - Interrupt]: %s\r\n", (char*) rx_data);
                        uart_write_bytes(UART_PORT_NUM, tx_buffer, echo_len);
                    }
                    break;
                }

                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "UART HW FIFO Overflow! Flushing...");
                    uart_flush_input(UART_PORT_NUM);
                    xQueueReset(uart_queue);
                    break;

                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "UART Ring Buffer Full! Flushing...");
                    uart_flush_input(UART_PORT_NUM);
                    xQueueReset(uart_queue);
                    break;

                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "UART Parity Error detected");
                    break;

                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "UART Frame Error detected");
                    break;

                default:
                    ESP_LOGI(TAG, "UART Event type: %d", event.type);
                    break;
            }
        }
    }
}

#else
#error "Invalid UART_MODE! Please choose 0 (Polling) or 1 (Interrupt)."
#endif
