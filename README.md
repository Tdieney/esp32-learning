# ESP32-S3 Embedded Systems Development

![Target](https://img.shields.io/badge/Target-ESP32--S3-blue?style=flat-square&logo=espressif)
![Framework](https://img.shields.io/badge/Framework-ESP--IDF_v5.2.0-red?style=flat-square&logo=espressif)
![Toolchain](https://img.shields.io/badge/Toolchain-xtensa--esp32s3--elf-green?style=flat-square)
![Language](https://img.shields.io/badge/Language-C%2FC%2B%2B-00599C?style=flat-square&logo=c%2B%2B)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

A comprehensive hands-on curriculum for mastering **ESP32-S3** microcontroller programming using the official **ESP-IDF** framework.

---

## Requirements & Setup

### Hardware Prerequisites
* **Development Board**: ESP32-S3-DevKitC-1 (Dual Type-C / USB Native JTAG)

### Software Prerequisites
* **Framework**: [ESP-IDF v5.2.0](https://docs.espressif.com/projects/esp-idf/en/v5.2/esp32s3/index.html)
* **Toolchain**: `xtensa-esp32s3-elf`
* **Editor**: VS Code

---

## Quick Start Guide

### 1. Initialize ESP-IDF Environment
Run the setup script in your terminal to export ESP-IDF path and toolchain:

* **Windows (Command Prompt / PowerShell)**:
  ```bat
  %USERPROFILE%\esp\esp-idf\export.bat
  ```
* **Linux / macOS**:
  ```bash
  . $HOME/esp/esp-idf/export.sh
  ```

### 2. Build and Flash a Project
Navigate into any lab project folder (e.g., `03_GPIO/BlinkLed`):

```bash
# 1. Set the target chip (one-time requirement per build directory)
idf.py set-target esp32s3

# 2. Configure project options (optional)
idf.py menuconfig

# 3. Build the project
idf.py build

# 4. Flash to board and monitor serial output
idf.py flash monitor
```

> 💡 **Bootloader Mode**: If automatic flashing fails, manually enter BOOT mode:
> **Hold BOOT button** → **Press RST button** → **Release RST** → **Release BOOT**.

---

## Code Style & Formatting

The repository enforces clean code standards with included configurations:
* `.clang-format` - Standardized C/C++ code formatting.
* `.clang-tidy` - Static analysis rules.

---

## License

This repository is released under the MIT License.
