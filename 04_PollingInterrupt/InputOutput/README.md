# GPIO Polling vs Interrupt

This example runs on an ESP32-S3 and shows the two basic GPIO modes of the ESP-IDF driver.
Select the mode with the `CONFIG_MODE` macro at the top of `main/main.c`:

| CONFIG_MODE | Mode | Behaviour |
|---|---|---|
| 0 | GPIO OUTPUT | Blinks the LED on GPIO4 every 500ms |
| 1 | GPIO INPUT | Button on GPIO0 toggles the LED **through an interrupt** |

## Interrupt mode (CONFIG_MODE 1)

The button is configured as an input with the internal pull-up enabled and
`GPIO_INTR_NEGEDGE`, so the interrupt fires on the falling edge (the moment the
active-low button is pressed).

Flow: **press -> falling edge -> ISR (set flag) -> `while(1)` in `app_main` (debounce & toggle LED)**

- `button_isr_handler()` runs in interrupt context: it simply sets `btn_pressed_flag = true` and returns immediately.
- The main loop in `app_main()` checks `btn_pressed_flag`, performs software debouncing (50ms delay), toggles the LED, and logs the state with `ESP_LOGI()`.
- This approach avoids FreeRTOS Queue and Task complexity while maintaining the key embedded rule: **Keep ISRs minimal and perform blocking/logging work in the main execution context.**

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
