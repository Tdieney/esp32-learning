/* Edge Impulse Espressif ESP32 Standalone Inference ESP IDF Example
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Include ----------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

#include "sdkconfig.h"
#include "esp_idf_version.h"

#include "driver/i2c.h"
#include "mpu6050.h"

static const char* TAG = "GESTURE_WAND";

#define I2C_MASTER_SCL_IO  9
#define I2C_MASTER_SDA_IO  8
#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000

static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = 0;

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

extern "C" void app_main()
{
    size_t idx = 0;

    mpu6050_handle_t mpu = NULL;

    ESP_ERROR_CHECK(i2c_master_init());

    mpu = mpu6050_create(I2C_MASTER_NUM, MPU6050_I2C_ADDRESS);

    if (mpu == NULL)
    {
        ESP_LOGE(TAG, "Failed to create MPU6050 handle");
        return;
    }
    ESP_ERROR_CHECK(mpu6050_wake_up(mpu));

    // Default config: 2g (1g ~ 9.8m/s^2)
    ESP_ERROR_CHECK(mpu6050_config(mpu, ACCE_FS_4G, GYRO_FS_500DPS));

    TickType_t last_wake = xTaskGetTickCount();
    const char* last_gesture = "";

    while (1)
    {
        mpu6050_acce_value_t accel;
        mpu6050_get_acce(mpu, &accel);
        features[idx++] = accel.acce_x;
        features[idx++] = accel.acce_y;
        features[idx++] = accel.acce_z;

        if (idx >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE)
        {
            idx = 0;
            // Raw data -> DSP (39 features) - Input NN
            signal_t signal;
            numpy::signal_from_buffer(features, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

            ei_impulse_result_t result = {0};
            run_classifier(&signal, &result, false);

            // ESP_LOGI(TAG, "Gia toc (g)   -> X: %5.2f | Y: %5.2f | Z: %5.2f", accel.acce_x, accel.acce_y,
            // accel.acce_z);

            for (uint8_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++)
            {
                if (result.classification[i].value > 0.8f && strcmp(result.classification[i].label, last_gesture) != 0)
                {
                    ESP_LOGI(TAG, "Detected: %s (%f)", result.classification[i].label, result.classification[i].value);

                    last_gesture = result.classification[i].label;
                }
            }
        }
        // 50 Hz
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(20));
    }
}
