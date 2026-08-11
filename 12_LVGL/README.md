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

### 5.12. ESP PSRAM (not under LVGL configuration — needed for larger draw buffers)
`Component config ---> ESP PSRAM`
- **`Support for external, SPI-connected RAM`** (`CONFIG_SPIRAM`): enable this first. Without it, `MALLOC_CAP_SPIRAM` allocations always fail and none of the options below exist in the menu.
- Then, inside the **`SPI RAM config`** submenu that appears:
  - **`Mode (QUAD/OCT) of SPI RAM chip in use`**: must match the physical PSRAM chip soldered on your module. This project uses **Octal** (`CONFIG_SPIRAM_MODE_OCT=y`). Picking the wrong mode makes PSRAM init fail (or the board hang) at boot — after flashing, always check the serial monitor log for a line confirming PSRAM was found before trusting the setting.
  - **`CONFIG_SPIRAM_SPEED`**: currently `40 MHz`. Many Octal PSRAM chips on ESP32-S3 support up to `80 MHz` — worth trying for extra bandwidth, but drop back to 40 MHz if PSRAM detection becomes unreliable.
  - `CONFIG_SPIRAM_USE_MALLOC=y` is the default once PSRAM is enabled — this is what lets `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` actually place an allocation in PSRAM instead of internal SRAM.
  - `CONFIG_SPIRAM_CLK_IO` / `CONFIG_SPIRAM_CS_IO` are fixed internal pins wired to the PSRAM die inside the module package (not regular usable GPIOs) — nothing to change here.

**Why this matters for LVGL (and why the draw buffers don't actually use it):** the two draw buffers in `init_lvgl()` are allocated from internal SRAM only (~512 KB total, shared with WiFi/FreeRTOS/app code), which is the reason they're capped at 1/10 of the screen (30 KB each) rather than full-screen. PSRAM (several MB) *would* remove that ceiling — this project tried exactly that (full-screen double buffer via `MALLOC_CAP_SPIRAM`) and hit three separate failures tracing back to one root issue: on this chip, a PSRAM-resident buffer is never "DMA-capable", so the SPI driver must bounce-copy every chunk through a fresh temporary allocation in the same fragmented internal SRAM pool it was meant to relieve — trading a real memcpy cost and heap-fragmentation fragility for a smaller LVGL-side win (fewer `flush_cb` calls). The draw buffers were reverted back to internal SRAM as a result. See **PSRAM Draw Buffers & SPI DMA** under [Hardcore Troubleshooting & Optimizations](#6-hardcore-troubleshooting--optimizations) for the full postmortem — it's a good read even though the code no longer takes this path, and PSRAM itself stays enabled here for other future uses (custom fonts, image assets, `lv_mem` pool — see Lesson 8).

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

### PSRAM Draw Buffers & SPI DMA
Moving the LVGL draw buffers into PSRAM (to get full-screen double buffering instead of 1/10-screen) surfaced two distinct failures, one right after the other. **This path was ultimately reverted** — see the verdict at the end of this section — but it's kept here because the bugs, and the reasoning for reverting, are worth understanding.

1. **Boot-time `assert failed: ... (buf1)`** — caused by allocating with `MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM` together. On the ESP32-S3, no single heap region carries both capability bits: internal SRAM is tagged `MALLOC_CAP_DMA` (not `MALLOC_CAP_SPIRAM`), and PSRAM is tagged `MALLOC_CAP_SPIRAM` (not `MALLOC_CAP_DMA`). Requesting both together always returns `NULL`. **Solution:** request `MALLOC_CAP_SPIRAM` alone — the S3's GDMA can already read PSRAM directly without the buffer also carrying the internal DMA tag.

2. **Task watchdog timeout on `IDLE1` once buf1/buf2 allocated fine** — this one is a hang, not a crash, so it's less obvious, and it took two attempts to pin down. The chain of cause and effect:
   - `esp_lcd_panel_io_spi` always splits a flush into chunks no larger than `spi_bus_config_t.max_transfer_sz`, no matter where the source buffer lives — this chunking happens regardless of buffer size or location.
   - A PSRAM address never passes the SPI driver's internal `esp_ptr_dma_capable()` check (only internal RAM does, `SOC_DMA_LOW`–`SOC_DMA_HIGH`), so every chunk is first bounce-copied into a temporary `MALLOC_CAP_DMA` buffer in internal SRAM, sized to `max_transfer_sz`, before DMA can send it.
   - The first fix attempt set `max_transfer_sz` to a moderate-looking 100 display lines (~94KB) — still not moderate enough, and still failed the same way. Adding a temporary check (`esp_lcd_panel_draw_bitmap`'s return code plus `heap_caps_get_free_size()` / `heap_caps_get_largest_free_block(MALLOC_CAP_DMA)` logged from `disp_flush_cb`) revealed the real number: **~120KB free overall, but the largest *contiguous* block was only ~31KB** — internal SRAM on this chip is handed out in several separate heap regions (see the `heap_init` log lines at boot), not one single pool, so total free bytes is the wrong number to size a single allocation against.
   - `spi_device_queue_trans()` then returns `ESP_ERR_NO_MEM` and nothing gets sent, so the `on_color_trans_done` callback that calls `lv_disp_flush_ready()` never fires.
   - LVGL's redraw loop blocks forever waiting for that flush-ready signal, pinning `gui_task` on Core 1 — starving the idle task on that core, which is why the board hits a repeating watchdog timeout instead of crashing once and rebooting cleanly.
   - Shrinking `max_transfer_sz` alone to 16 display lines (~15KB, comfortably under the ~31KB largest-block ceiling) *still* failed with `ESP_ERR_NO_MEM`. The missing piece: `panel_io_spi_tx_color()` queues chunks back-to-back with **no wait in between** until `esp_lcd_panel_io_spi_config_t.trans_queue_depth` transactions are simultaneously in flight, and each queued chunk's bounce buffer stays allocated until its transfer physically completes, not until it's merely queued. Worst-case simultaneous internal-SRAM usage is therefore `max_transfer_sz * trans_queue_depth`, not just `max_transfer_sz` — with `trans_queue_depth = 10` (the original value), a 15KB chunk size could still ask for ~150KB of bounce buffers all in flight at once.
   - **Solution:** budget `max_transfer_sz * trans_queue_depth` against the *largest contiguous free block* under `MALLOC_CAP_DMA` (`heap_caps_get_largest_free_block()`), with a healthy safety margin — it only shrinks further as the app runs and the heap fragments more. This project settled on 16 display lines (~15KB) with `trans_queue_depth = 3` (~45KB worst case), against a measured ~31KB single-block / ~70-120KB total free at boot.
   - The diagnostic logging added to `disp_flush_cb` (error name + free/largest-block size on any `esp_lcd_panel_draw_bitmap` failure) was kept in the code rather than removed — it turns any future occurrence of this class of bug into an immediate log message instead of an opaque watchdog dump.

**Verdict — reverted back to internal SRAM:** even with a working `max_transfer_sz` / `trans_queue_depth` combination, the PSRAM path adds a real CPU memcpy on every single SPI chunk that a plain internal-SRAM buffer never needed at all (internal SRAM already passes `esp_ptr_dma_capable()`, so DMA reads it directly, zero-copy). The only actual win was LVGL calling `flush_cb` once per redraw instead of ~10 times; the cost was a per-flush memcpy tax plus an ongoing fragility (the safe chunk-size/queue-depth budget only shrinks as more widgets and animations fragment the same internal heap further in later lessons). That trade wasn't worth it here, especially since the screen flicker that motivated this whole investigation had already been traced to a power-supply issue, not buffer size (FPS stayed rock-stable at 33 throughout). `main.c` now allocates `buf1`/`buf2` back in internal SRAM (`MALLOC_CAP_DMA`, 1/10-screen, zero-copy) as it originally did. PSRAM itself is left enabled in `menuconfig` (§5.12) for other future uses — see Lesson 8 (custom fonts, image assets, RAM/PSRAM optimization) — just not for these draw buffers.

---
**Build Command:**
```bash
idf.py build flash monitor
```
