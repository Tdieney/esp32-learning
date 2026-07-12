# Development & Learning Plan: ST7796U Display + FT6336U Touch + LVGL

The goal of this project is to successfully interface an ST7796U SPI display and an FT6336U I2C touch controller with the ESP32-S3, integrate the LVGL graphics library, and learn how to use LVGL to build professional User Interfaces (UI).

## Phase 1: Basic Hardware Initialization (Without LVGL)

Objective: Ensure basic communication with the display and touch controller works independently.

### 1.1. Hardware & Communication Setup
- [x] **ST7796U Display (SPI):**
  - [x] Configure SPI pins (MOSI, MISO, SCLK, CS).
  - [x] Configure control pins (DC, RST, Backlight).
  - [x] Initialize SPI bus using ESP-IDF (`spi_master`).
- [x] **FT6336U Touch (I2C):**
  - [x] Configure I2C pins (SDA, SCL, RST, INT).
  - [x] Initialize I2C bus using ESP-IDF (`i2c_master`).

### 1.2. Basic Display Driver
- [x] Use ESP-IDF's `esp_lcd_st7796` component.
- [x] Write display initialization function.
- [x] **Goal:** Successfully power on the display and fill the screen with solid colors to verify functionality.

### 1.3. Basic Touch Driver
- [x] Use `esp_lcd_touch_ft5x06` component.
- [x] Integrate hardware interrupt processing from the INT pin.
- [x] **Goal:** Print exact X, Y coordinates to the terminal log upon screen touch.

---

## Phase 2: LVGL Integration into ESP-IDF

This phase introduces the LVGL UI engine to the project, bridging the display and touch components established above.

### 2.1. Add LVGL Component
- [x] Add the `lvgl` library via `idf_component.yml` (IDF Component Manager).
- [x] Configure LVGL parameters (screen resolution, color depth) via `menuconfig`.

### 2.2. Display Porting (Interface)
- [x] Allocate Draw Buffers (Double Buffering) using DMA to offload SPI transfers from the CPU.
- [x] Write `flush_cb`: The callback LVGL uses to render areas of the screen.
- [x] Register the display driver with LVGL (`lv_disp_drv_t`).

### 2.3. Touch Porting (Input Device Interface)
- [x] Write `read_cb`: Link the FT6336U X, Y coordinate reading function.
- [x] Register the input device driver with LVGL (`lv_indev_drv_t` in `LV_INDEV_TYPE_POINTER` mode).

### 2.4. LVGL System Tick & Task
- [x] Configure a timer or call `lv_tick_inc()` to keep track of LVGL's internal time (for animations, input lag, etc.).
- [x] Create a dedicated FreeRTOS Task (`gui_task`) on Core 1 to run `lv_timer_handler()` periodically.

### 2.5. First UI Test (Hello World LVGL)
- [x] Create a basic Button object in the center of the screen.
- [x] Click the button to verify display and touch interaction via terminal logs.
- [x] Run LVGL Built-in Demos (`lv_demo_widgets`, `lv_demo_music`).

---

## 🚀 Practical Engineering Lessons (Phase 2 Insights)
During development, several critical low-level issues were identified and resolved:
1. **Screen Tearing:** Never call `lv_disp_flush_ready()` immediately inside `flush_cb` when using DMA. It must be called inside the SPI `on_color_trans_done` interrupt callback to prevent LVGL from overwriting the buffer mid-transfer.
2. **Corrupted/Pixelated Colors:** This is caused by SPI Endianness mismatch (ESP32 is Little-Endian, SPI Display expects Big-Endian). Fixed by enabling `Swap the 2 bytes of RGB565 color` in `menuconfig`.
3. **Display Driver Ignoring Commands:** The 3rd-party display driver ignored standard ESP-IDF `swap_xy` commands for landscape rotation. Fixed by sending a hardware `MADCTL` command (0x36 = 0x20 | 0x08) directly to the panel.
4. **Touch Coordinate Misalignment:** Combining `esp_lcd_touch` with screen rotation caused incorrect scaling/offset math within the driver. Fixed by disabling driver-level mirroring and manually mapping the coordinates (e.g., `480 - raw_x`) inside `touch_read_cb`.
5. **Insufficient Memory for Demos:** The default LVGL memory pool is 32KB. Fixed by increasing `CONFIG_LV_MEM_SIZE` to 64KB in `menuconfig` and enabling Montserrat 12/16 fonts for the Music Demo.
6. **Smooth Performance Optimization:** Enabled **Double Buffering** (buf1 & buf2) for seamless DMA transfers, pinned the LVGL Task to **Core 1** to prevent system interruptions, and maximized SPI Clock to **80 MHz** by utilizing the ESP32-S3's native IOMUX pins.

---

## Phase 3: LVGL Mastery Roadmap

A step-by-step plan to build professional UIs with LVGL.

### Lesson 1: Core Concepts
- **Object/Widget:** Everything inherits from `lv_obj_t`. Creation, deletion, and lifecycle management.
- **Parent-Child Tree:** Screen -> Container -> Button -> Label.
- **Position & Size:** `lv_obj_set_size`, `lv_obj_set_pos`.
- **Parts & States:** Widget anatomy (e.g., Slider track, knob) and states (Default, Pressed, Disabled).

### Lesson 2: Layout & Alignment
- **Alignment:** Basic alignment using `lv_obj_align` (e.g., center, corner).
- **Flexbox (`LV_LAYOUT_FLEX`):** Auto-arrange widgets in rows/columns (similar to Web Flexbox).
- **Grid (`LV_LAYOUT_GRID`):** Split the screen into structured grids (similar to CSS Grid).

### Lesson 3: Styling
- Introduction to `lv_style_t`.
- Modifying Backgrounds, Text colors, Borders, Radius, and Shadows.
- Gradients for premium buttons.
- Local Styles vs. Shared Styles.

### Lesson 4: Event Management
- Registering events: `lv_obj_add_event_cb`.
- Event codes: `LV_EVENT_CLICKED`, `LV_EVENT_VALUE_CHANGED`, etc.
- Accessing target objects and passing `user_data`.

### Lesson 5: Common Widgets Exploration
- **Informational:** Label, Line, Bar, Spinner.
- **Interactive:** Button, Switch, Checkbox.
- **Input:** Slider, Roller, Dropdown, Keyboard.
- **Data Display (IoT):** Chart, Meter.

### Lesson 6: Animations & Transitions
- **Transitions:** Smooth auto-transitions between states (e.g., button color fade on press).
- **Animations (`lv_anim_t`):** Programmatic movements (e.g., sliding menus, popups).

### Lesson 7: Multi-Screen Handling
- Multi-screen architecture.
- Screen transition animations: `lv_scr_load_anim`.
- Memory management during transitions (auto deletion).

### Lesson 8: Advanced Tools
- Custom Fonts.
- Images and Icons.
- **SquareLine Studio / LVGL UI Creator:** Drag-and-drop UI design tools to auto-generate C code.
- RAM, PSRAM, and Flash optimization on ESP32.
