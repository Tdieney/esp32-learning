# Overview

## 1. Hardware Architecture

### Overview

A Microcontroller Unit (MCU) is a System-on-Chip (SoC) optimized for real-time operation and low power consumption. It integrates a CPU, memory (Flash/SRAM), and peripherals into a single chip.

### ESP32-S3 Specifications

* **CPU**: Dual-core 32-bit Xtensa LX7 running at up to **240 MHz**
* **Memory**:
  * 384 KB ROM
  * 512 KB SRAM (including 16 KB dedicated RTC SRAM)
* **Connectivity**:
  * Integrated 2.4 GHz Wi-Fi (802.11 b/g/n)
  * Bluetooth 5.0 LE / Mesh
* **Peripherals**:
  * 45 GPIOs
  * Native USB 2.0 Full-Speed support
  * 2 × 12-bit ADCs
  * Hardware cryptographic accelerators (AES, SHA, RSA, RNG)
  * Dedicated display and vision peripherals (LCD Interface, DVP Camera Interface)

### Practical Insights

#### Symmetric Multiprocessing (SMP)

The ESP32-S3 uses a Harvard architecture and features two identical cores in a **Symmetric Multiprocessing (SMP)** configuration. While ESP-IDF maintains legacy macros for backward compatibility, they are officially Core 0 and Core 1:

* `PRO_CPU` (Core 0) – Traditionally handles the protocol stacks (Wi-Fi/BT).
* `APP_CPU` (Core 1) – Traditionally handles user applications.

Because the cores are symmetric, the scheduler is highly flexible. However, pinning computationally intensive tasks to `APP_CPU` using `xTaskCreatePinnedToCore()` remains a best practice to prevent application logic from starving the networking operations on `PRO_CPU`.

#### Why LX7 Matters

One of the biggest advantages of the LX7 core over the LX6 core (used in the original ESP32) is its support for **Vector Instructions**.

For Edge AI workloads such as face recognition and voice processing using ESP-DL, these vector instructions significantly accelerate matrix operations and inference performance.

---

## 2. Power and Memory Management

### Power Modes

The ESP32-S3 supports several power-saving modes:
* Active Mode
* Modem Sleep
* Light Sleep
* Deep Sleep

### RTC Power Domain

The RTC domain contains:
* PMU (Power Management Unit)
* RTC Memory (16 KB)
* Ultra-Low-Power (ULP) Coprocessor

### Practical Insights

#### Deep Sleep and State Retention

When entering Deep Sleep, the main CPUs and SRAM are powered off. Any data stored in regular RAM is lost.

To preserve variables across wake-up cycles, they must be declared using the `RTC_DATA_ATTR` attribute so they are placed inside RTC SRAM.

#### Leveraging the ULP Coprocessor

Do not overlook the ULP coprocessor. The S3 supports both **ULP-FSM** (programmed in assembly) and a much more accessible **ULP-RISC-V** (programmed in standard C).

For battery-powered applications such as IoT sensors or touch-based devices, the main CPU can remain in Deep Sleep while the ULP periodically wakes up to:
* Read ADC values
* Scan touch sensors
* Monitor thresholds

The main CPU is only awakened when necessary. This approach can extend battery life from weeks to years.

---

## 3. Documentation Mastery

The ESP-IDF programming guide contains everything you need to develop applications for the ESP32-S3, including API references, hardware overviews, and examples.

You can access the official documentation here:
[ESP-IDF Programming Guide (ESP32-S3)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/index.html)

---

## 4. Ecosystem and Development Toolchain

The development ecosystem for ESP32-S3 is based on **ESP-IDF** (Espressif IoT Development Framework). 

A ready-to-build project is available in the `BlinkLED` folder.

For a complete guide on how to install the toolchain, set up VSCode, and configure the first project, please refer to the dedicated setup guide:
[Setup Dev Environment](Setup_Env.md)
