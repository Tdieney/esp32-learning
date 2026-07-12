# ESP32-S3 LVGL Integration Guide (ST7796 + FT6336U)

A comprehensive, production-ready guide to setting up an ESP32-S3 with an SPI ST7796 display, an I2C FT6336U capacitive touch controller, and the LVGL graphics library using the modern ESP-IDF `esp_lcd` framework.

## 1. Project Initialization

Initialize your ESP-IDF environment and create the project structure for the ESP32-S3 target.

```bash
# Export ESP-IDF variables
export.bat  # or . ./export.sh on Linux/Mac

# Create and configure project
idf.py create-project lvgl_practice
cd lvgl_practice
idf.py set-target esp32s3
```

## 2. Dependencies & Component Management

Instead of writing bare-metal drivers, this project leverages the official ESP Component Registry for robust abstraction. Run these commands to add the necessary dependencies to your `idf_component.yml`:

```bash
# Add ST7796 LCD display driver
idf.py add-dependency "espressif/esp_lcd_st7796"

# Add FT6336U Touch driver (uses the FT5x06 architecture component)
idf.py add-dependency "espressif/esp_lcd_touch_ft5x06"

# Add LVGL library (v8.3.x recommended for stability)
idf.py add-dependency "lvgl/lvgl^8.3.0"
```

## 3. Hardware Pinout (ESP32-S3 IOMUX)

To achieve the maximum 80MHz SPI clock speed, the SPI pins *must* be routed through the ESP32-S3's dedicated hardware IOMUX pins rather than the generic GPIO matrix.

| Interface | Signal | ESP32-S3 Pin | Notes |
| :--- | :--- | :--- | :--- |
| **SPI2 (LCD)** | CLK | GPIO 12 | Native IOMUX FSPICLK |
| | MOSI | GPIO 11 | Native IOMUX FSPID |
| | MISO | GPIO 13 | Native IOMUX FSPIQ (Optional) |
| | CS | GPIO 10 | Native IOMUX FSPICS0 |
| | DC/RS | GPIO 9 | Data/Command |
| | RST | GPIO 14 | Display Reset |
| | BL | GPIO 2 | Backlight Control |
| **I2C0 (Touch)**| SDA | GPIO 4 | Pull-up required |
| | SCL | GPIO 5 | Pull-up required |
| | INT | GPIO 7 | Touch Interrupt |
| | RST | GPIO 6 | Touch Reset |

## 4. Software Architecture (`main.c` Workflow)

A professional ESP-IDF LVGL application follows a strict initialization sequence:

1. **Bus Initialization:** 
   - Configure the SPI bus with `SPI_DMA_CH_AUTO` and a sufficiently large `max_transfer_sz`.
   - Configure the I2C bus at 400kHz for the touch controller.
2. **Display (`esp_lcd`) Initialization:**
   - Attach an SPI IO handle with `esp_lcd_new_panel_io_spi`.
   - Create the ST7796 panel and explicitly override the `MADCTL` register for forced Landscape orientation.
3. **Touch (`esp_lcd_touch`) Initialization:**
   - Attach an I2C IO handle and instantiate the FT5x06 touch driver.
   - *Crucial:* Set `scl_speed_hz = 0` in the IO config to prevent conflicts with legacy I2C drivers in ESP-IDF v5.2+.
4. **LVGL Core Setup:**
   - Allocate two DMA-capable draw buffers (Double Buffering) using `heap_caps_malloc(..., MALLOC_CAP_DMA)`.
   - Bind the display flush callback and the touch read callback to LVGL.
5. **FreeRTOS Multithreading:**
   - Pin the `gui_task` to **Core 1** to isolate UI rendering from Core 0's WiFi/System interrupts.
   - Run `lv_timer_handler()` periodically inside the task.

## 5. Build Configuration (`menuconfig`)

Almost every LVGL-specific setting lives under one Kconfig menu:

```bash
idf.py menuconfig
```

```
Component config  --->  LVGL configuration  --->
```

Tip: inside `menuconfig`, press `/` and type a symbol name (e.g. `LV_USE_CHART`) to jump straight to it instead of drilling through submenus.

### 5.1. Color settings
`LVGL configuration ---> Color settings`
- **Color depth:** `16: RGB565` — must match `bits_per_pixel = 16` in `esp_lcd_panel_dev_config_t` (`main.c`).
- **`LV_COLOR_16_SWAP`** ("Swap the 2 bytes of RGB565 color"): **must be ON**. The ESP32 is little-endian but the ST7796 SPI interface expects big-endian pixel data — without this the screen shows corrupted/noisy colors (see **Hardcore Troubleshooting & Optimizations** below).

### 5.2. Memory settings
`LVGL configuration ---> Memory settings`
- **`LV_MEM_SIZE_KILOBYTES`** ("Size of the memory used by `lv_mem_alloc`"): this project currently runs at **128 KB**, the maximum the built-in allocator's Kconfig range allows (2–128). Bump usage down only if you need the RAM elsewhere; if you ever see `lv_mem_alloc` failures in the log, the fonts/widgets you enabled need more than 128 KB and you'd need a custom allocator (`LV_MEM_CUSTOM`) instead.
- **`LV_MEM_BUF_MAX_NUM`** (default 16): number of scratch buffers used internally during rendering (shadows, gradients, transforms). If the log warns about running out of intermediate buffers, raise this.
- Note: this memory pool is separate from the two DMA draw buffers allocated manually in `init_lvgl()` via `heap_caps_malloc(..., MALLOC_CAP_DMA)` — those aren't configured here, they're sized directly in code (`LCD_H_RES * LCD_V_RES / 10`).

### 5.3. HAL Settings
`LVGL configuration ---> HAL Settings`
- **`LV_DISP_DEF_REFR_PERIOD`** / **`LV_INDEV_DEF_READ_PERIOD`** (ms): default periods LVGL uses to redraw/read input when a driver doesn't override them. In `main.c`, `gui_task` calls `lv_tick_inc(10)` + `lv_timer_handler()` every 10 ms — keep these in sync if you ever change that task's `vTaskDelay`.
- **`LV_DPI_DEF`**: baseline DPI LVGL uses to compute default widget sizes/paddings. Rarely needs touching.

### 5.4. Feature configuration → Logging
`LVGL configuration ---> Feature configuration ---> Logging`
- **`LV_USE_LOG`**: off by default. Turn it on (verbosity `Info` or `Trace`) while debugging object lifecycle issues (e.g. Lesson 1's create/delete) — LVGL prints its own internal logs alongside your `ESP_LOGI` calls.

### 5.5. Feature configuration → Others (debug overlays)
`LVGL configuration ---> Feature configuration ---> Others`
- **`LV_USE_PERF_MONITOR`** ("Show CPU usage and FPS count"): **ON** in this project — draws an FPS/CPU overlay (bottom-right corner). Watch this during the Styling and Animation lessons to see the real cost of shadows, gradients, and `lv_anim_t`.
- **`LV_USE_MEM_MONITOR`**: off. Turn it on to watch the `lv_mem_alloc` pool usage live while creating/deleting objects in Lesson 1.

### 5.6. Font usage
`LVGL configuration ---> Font usage`
- **Enable built-in fonts:** `Montserrat 12`, `14`, and `16` are currently enabled. Each extra size/weight adds flash usage — only enable what a given UI actually needs.
- **Select theme default title font:** currently `Montserrat 14` — the fallback font used by `lv_label_create()` when no style overrides it (relevant in Lesson 3: Styling).

### 5.7. Widget usage / Extra Widgets
`LVGL configuration ---> Widget usage` and `---> Extra Widgets`

Every widget is its own Kconfig symbol, so you only pay flash cost for what you use. All of the following are already **enabled** in this project:

| Menu | Widgets already ON |
| :--- | :--- |
| Widget usage | Arc, Bar, Button, Button matrix, Canvas, Checkbox, Dropdown, Image, Label, Line, Roller, Slider, Switch, Textarea, Table |
| Extra Widgets | Anim image, Calendar, **Chart**, Colorwheel, Imgbtn, Keyboard, LED, List, Menu, **Meter**, Msgbox, Span, Spinbox, Spinner, Tabview, Tileview, Win |

Lesson 5 (Common Widgets) will use most of these directly — **Chart** and **Meter** in particular are already turned on, so no extra `menuconfig` work is needed once we get there. If flash ever gets tight, this is the first place to prune widgets you don't use.

### 5.8. Themes
`LVGL configuration ---> Themes`
- **`LV_USE_THEME_DEFAULT`**: ON — gives every widget a complete default look (colors, radius, shadows) without writing any style code.
- **`LV_THEME_DEFAULT_DARK`**: toggles the light/dark variant of the default theme.
- **`LV_THEME_DEFAULT_GROW`** / **`LV_THEME_DEFAULT_TRANSITION_TIME`**: the built-in "grow on press" effect and its transition duration (ms) — this is where LVGL's default press animation comes from, useful background for Lesson 3 (Styling) and Lesson 6 (Animations).

### 5.9. Layouts
`LVGL configuration ---> Layouts`
- **`LV_USE_FLEX`** and **`LV_USE_GRID`**: both **ON** — required for Lesson 2 (Flexbox/Grid layout). Nothing to change here.

### 5.10. Demos
`LVGL configuration ---> Demos`
- **`LV_USE_DEMO_WIDGETS`** and **`LV_USE_DEMO_MUSIC`** are both currently **ON**. In `main.c`, the `#if defined(CONFIG_LV_USE_DEMO_MUSIC) ... #elif defined(CONFIG_LV_USE_DEMO_WIDGETS) ... #else` chain checks Music first, so right now the built-in Music Player demo runs instead of any custom lesson code.
- **Action needed before Lesson 1:** disable both `LV_USE_DEMO_WIDGETS` and `LV_USE_DEMO_MUSIC` here, so the `#else` branch — where hand-written lesson UI code lives — actually executes.

### 5.11. Compiler optimization (not under LVGL configuration)
This one lives one level up: `Component config ---> Compiler options ---> Optimization Level`. Set it to **`Optimize for performance (-O2)`** for the smoothest FPS — it's a global ESP-IDF setting, not LVGL-specific.

### Keeping menuconfig changes reviewable
`sdkconfig` is a large generated file. After running `menuconfig`, run `git diff sdkconfig` (or `idf.py save-defconfig` to produce a minimal `sdkconfig.defaults`) so you can see exactly which options changed.

## 6. Hardcore Troubleshooting & Optimizations

This project was built practically, solving several deep low-level issues. If you adapt this code to other boards, keep these lessons in mind:

### Display & Touch Glitches
- **Screen Tearing / Glitching:** If you call `lv_disp_flush_ready()` immediately after triggering an `esp_lcd` DMA transfer, LVGL will overwrite the buffer while DMA is still reading it. **Solution:** Register an `on_color_trans_done` SPI interrupt callback and trigger `lv_disp_flush_ready()` from there.
- **Washed-out / Blocky Colors:** The ESP32 is Little-Endian, but SPI displays expect Big-Endian data. **Solution:** You must enable 16-bit color swapping in LVGL `menuconfig`, otherwise colors will look like digital noise.
- **Driver Ignoring Rotation Commands:** Many 3rd-party `esp_lcd` vendor drivers (like ST7796) ignore the standard `esp_lcd_panel_swap_xy` commands. **Solution:** Bypass the API and send a direct SPI transaction to the `MADCTL` (0x36) hardware register (e.g., sending `0x20 | 0x08` for landscape BGR).
- **Touch Coordinates Misaligned / Inverted Axis:** Applying rotation via `esp_lcd_touch` can cause scaling math errors if the driver misidentifies the maximum physical resolution. **Solution:** Disable driver-level mirroring (`mirror_x = 0`, `mirror_y = 0`) and perform absolute coordinate mapping manually inside the LVGL `touch_read_cb` (e.g., `mapped_x = 480 - raw_x`).

### Performance Maximization
- **Double Buffering:** Allocating two separate LVGL draw buffers (`buf1` and `buf2`) allows the CPU to render frame *N+1* while the DMA controller asynchronously transmits frame *N* to the display.
- **80MHz SPI Clock:** SPI throughput is capped at 40MHz if routed through the ESP32's GPIO Matrix. By strictly using the native IOMUX pins, the `pclk_hz` can be safely pushed to `80 * 1000 * 1000`.
- **Core Affinity:** Assigning `xTaskCreatePinnedToCore` to `Core 1` guarantees the UI thread is never interrupted by background RTOS operations.

---
**Build Command:**
```bash
idf.py build flash monitor
```
