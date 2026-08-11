#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"

// esp_lcd framework includes
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
// Component includes (ensure you have added them via idf.py add-dependency)
#include "esp_lcd_st7796.h"
#include "esp_lcd_touch_ft5x06.h"
#include "lvgl.h"

#include "ui/ui.h"

static const char* TAG = "main";

// ==========================================
// PIN CONFIGURATIONS (Adjust for your board)
// ==========================================
// SPI pins for LCD
#define LCD_HOST         SPI2_HOST
#define LCD_PIN_MOSI     11
#define LCD_PIN_MISO     13 // ST7796 typically doesn't need MISO for just drawing
#define LCD_PIN_CLK      12
#define LCD_PIN_CS       10
#define LCD_PIN_DC       9 // RS
#define LCD_PIN_RST      14
#define LCD_PIN_BK_LIGHT 2

// I2C pins for Touch
#define I2C_HOST_PORT 0
#define TOUCH_PIN_SDA 4
#define TOUCH_PIN_SCL 5
#define TOUCH_PIN_INT 7 // Interrupt pin for touch (triggers when touched)
#define TOUCH_PIN_RST 6 // Reset pin for touch

// Display resolution (Landscape mode)
#define LCD_H_RES 480
#define LCD_V_RES 320

// Global handles
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

// ==========================================
// 3.1. BUS INITIALIZATION
// ==========================================
void init_buses(void)
{
    ESP_LOGI(TAG, "Initialize SPI bus for LCD");
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_CLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = LCD_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        // Max transfer size (64KB is enough for LVGL draw buffer)
        .max_transfer_sz = LCD_H_RES * 100 * sizeof(uint16_t),
    };
    // Use an auto DMA channel so the CPU isn't blocked during drawing
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Initialize I2C bus for Touch");
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TOUCH_PIN_SDA,
        .scl_io_num = TOUCH_PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000, // I2C speed at 400kHz
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_HOST_PORT, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_HOST_PORT, i2c_conf.mode, 0, 0, 0));
}

// ==========================================
// 3.2. DISPLAY INITIALIZATION (esp_lcd)
// ==========================================
void init_display(void)
{
    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = 80 * 1000 * 1000, // 80 MHz is typical for ST7796, adjust if unstable
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t) LCD_HOST, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install ST7796 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR, // ST7796 usually uses BGR, swap to RGB if colors are wrong
        .bits_per_pixel = 16,             // RGB565
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));

    // --- FORCE SCREEN ROTATION VIA HARDWARE COMMAND (MADCTL) ---
    // The 3rd-party ST7796 driver often ignores the swap_xy command, so we send the command
    // directly to register 0x36 instead.
    // Bits: MY(0x80), MX(0x40), MV(0x20) (Swap XY), BGR(0x08).

    // uint8_t madctl_val = 0x20 | 0x08; // Landscape (MV=1) with BGR color order.
    uint8_t madctl_val = 0x20 | 0x80 | 0x40 | 0x08;
    // If the screen appears mirrored or upside down, add 0x80 or 0x40.
    // Example: madctl_val = 0x20 | 0x80 | 0x08;

    esp_lcd_panel_io_tx_param(io_handle, 0x36, &madctl_val, 1);

    ESP_LOGI(TAG, "Turn on LCD backlight");
    if (LCD_PIN_BK_LIGHT >= 0)
    {
        gpio_set_direction(LCD_PIN_BK_LIGHT, GPIO_MODE_OUTPUT);
        gpio_set_level(LCD_PIN_BK_LIGHT, 1); // Turn on (1 or 0 depending on circuit)
    }

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

// ==========================================
// 3.3. TOUCH INITIALIZATION (esp_lcd_touch)
// ==========================================
void init_touch(void)
{
    ESP_LOGI(TAG, "Initialize FT6336U (FT5x06 driver) touch controller");

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    tp_io_config.scl_speed_hz = 0; // Fixes a clock-speed conflict with the legacy I2C driver on ESP-IDF v5.2+
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t) I2C_HOST_PORT, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_PIN_RST,
        .int_gpio_num = TOUCH_PIN_INT,
        .flags =
            {
                // Disable the driver's built-in mirroring to avoid library bugs
                // We flip the coordinates manually below for 100% accuracy
                .swap_xy = 1,
                .mirror_x = 0,
                .mirror_y = 0,
            },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &touch_handle));
}

// ==========================================
// 3.4. LVGL PORTING
// ==========================================
static void disp_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // Push the pixel data to the display (via DMA)
    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    if (ret != ESP_OK)
    {
        // Without this check, a draw_bitmap failure hangs the board forever waiting on a
        // flush_ready that will never come, with zero indication of why. Log the real reason,
        // plus how much contiguous internal DMA-capable RAM is available, in case it recurs.
        ESP_LOGE(TAG, "esp_lcd_panel_draw_bitmap failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "MALLOC_CAP_DMA free: %u bytes, largest block: %u bytes",
                 (unsigned) heap_caps_get_free_size(MALLOC_CAP_DMA),
                 (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    }

    // DO NOT call lv_disp_flush_ready() here!
    // Because DMA runs in the background, reporting "done" immediately would let LVGL overwrite
    // the buffer while DMA is still sending it, causing visual glitches (tearing, misplacement).
}

// This callback is automatically invoked by the SPI interrupt when DMA finishes sending the frame
static bool
notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t* edata, void* user_ctx)
{
    lv_disp_drv_t* disp_drv = (lv_disp_drv_t*) user_ctx;
    lv_disp_flush_ready(disp_drv);
    return false;
}

static void touch_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
    esp_lcd_touch_handle_t touch_handle = (esp_lcd_touch_handle_t) drv->user_data;
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    esp_lcd_touch_read_data(touch_handle);
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(touch_handle, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0)
    {
        data->state = LV_INDEV_STATE_PR;

        // --- MANUAL COORDINATE MAPPING ---
        // Read the raw coordinates (already swap_xy = 1)
        int raw_x = touchpad_x[0];
        int raw_y = touchpad_y[0];

        // Enabling mirror_x = 1 previously produced out-of-range values (-142 to 314) because the
        // driver's x_max was incorrectly set to 320.
        // We now disable mirror_x in the driver and flip X manually here using the correct value of 480
        data->point.x = 480 - raw_x;
        data->point.y = raw_y; // Y is kept as-is, since it already runs correctly from 20 -> 313

        // Clamp so the value never goes negative or exceeds the screen bounds
        // if (data->point.x < 0)
        //     data->point.x = 0;
        // if (data->point.x > 479)
        //     data->point.x = 479;
        // if (data->point.y < 0)
        //     data->point.y = 0;
        // if (data->point.y > 319)
        //     data->point.y = 319;

        // Log the processed coordinates for verification
        // ESP_LOGI(TAG, "Touch -> Raw X:%d Y:%d | LVGL X:%d Y:%d", raw_x, raw_y, data->point.x, data->point.y);
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

static void gui_task(void* arg)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);    // Tell LVGL that 10ms have elapsed
        lv_timer_handler(); // Process UI rendering and events
    }
}

void init_lvgl(void)
{
    ESP_LOGI(TAG, "Initialize LVGL");
    lv_init();

    ESP_LOGI(TAG, "Allocate LVGL draw buffers (Double Buffering)");
    // Increase the buffer size to 1/5 of the screen (if RAM allows) for smoother rendering, or keep it at 1/10
    size_t draw_buf_size = LCD_H_RES * LCD_V_RES / 10;

    // Allocate Buffer 1
    lv_color_t* buf1 = (lv_color_t*) heap_caps_malloc(draw_buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);

    // Allocate Buffer 2 (this is where the magic happens: LVGL draws into buf2 while DMA sends buf1)
    lv_color_t* buf2 = (lv_color_t*) heap_caps_malloc(draw_buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);

    static lv_disp_draw_buf_t disp_buf;
    // Initialize both buffers for LVGL
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, draw_buf_size);

    ESP_LOGI(TAG, "Register LVGL display driver");
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle; // Pass in the display panel handle
    lv_disp_drv_register(&disp_drv);

    // Register the callback that signals when the SPI DMA transfer is complete
    esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_lvgl_flush_ready,
    };
    esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, &disp_drv);

    ESP_LOGI(TAG, "Register LVGL touch driver");
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read_cb;
    indev_drv.user_data = touch_handle; // Pass in the touch controller handle
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "Create Smart Home Hub UI");
    ui_init();

    ESP_LOGI(TAG, "Create LVGL task on Core 1");
    // Pin the UI task to Core 1 (Core 0 is reserved for WiFi and system tasks)
    xTaskCreatePinnedToCore(gui_task, "gui_task", 4096 * 2, NULL, 5, NULL, 1);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting LVGL Practice");

    // 3.1. Initialize Buses
    init_buses();

    // 3.2. Initialize Display
    init_display();

    // 3.3. Initialize Touch
    init_touch();

    ESP_LOGI(TAG, "Hardware initialized successfully!");

    // 3.4. Initialize LVGL
    init_lvgl();
}
