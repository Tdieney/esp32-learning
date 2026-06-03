# Register-Level GPIO

This example demonstrates how to control a GPIO pin at the register level on an ESP32-S3.
It configures GPIO4 as an input with a pull-up resistor and GPIO45 as an output to control a LED.

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
