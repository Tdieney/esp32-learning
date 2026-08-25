#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "mpu6050.h"
#include "esp_log.h"

static const char* TAG = "MPU6050";

#define I2C_MASTER_SCL_IO  9
#define I2C_MASTER_SDA_IO  8
#define I2C_MASTER_NUM     I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

void app_main(void)
{
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

    mpu6050_acce_value_t accel;
    TickType_t last_wake = xTaskGetTickCount();

    while (1)
    {
        mpu6050_get_acce(mpu, &accel);

        // ESP_LOGI(TAG, "Gia toc (g)   -> X: %5.2f | Y: %5.2f | Z: %5.2f", accel.acce_x, accel.acce_y, accel.acce_z);

        /* Send via Serial to Edge Impulse Data Forwarder: X,Y,Z */
        printf("%.4f,%.4f,%.4f\n", accel.acce_x, accel.acce_y, accel.acce_z);

        // 50 Hz
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(20));
    }
}
