# Register-Level GPIO

This example demonstrates how to control a GPIO pin at the register level on an ESP32-S3.

## How it works

Instead of using the high-level `driver/gpio.h` HAL, this code uses direct register access via `REG_WRITE` and constants from `soc/gpio_reg.h`.

## Building the project

1. Set the target to ESP32-S3:
   ```bash
   idf.py set-target esp32s3
   ```
2. Build the project:
   ```bash
   idf.py build
   ```
3. Flash and monitor:
   ```bash
   idf.py flash monitor
   ```
