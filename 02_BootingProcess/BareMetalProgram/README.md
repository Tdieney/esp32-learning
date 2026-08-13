# ESP32-S3 bare-metal blink

This is a minimal bare-metal LED blink project for the ESP32-S3-DevKitC-1.

It uses:

- one external single-color LED on GPIO4;
- C for the application;
- a small Xtensa assembly startup file;
- no Arduino, FreeRTOS, ESP-IDF runtime, or C standard library.

ESP-IDF is used only as a convenient source of the compiler and `esptool`.

## Hardware

Connect the LED as follows:

```text
GPIO4 ---- 330 ohm resistor ---- LED anode
                                  LED cathode ---- GND
```

The resistor is required. The firmware drives GPIO4 high to turn the LED on.

## Build

From Command Prompt:

```bat
cd 02_BootingProcess\BareMetalProgram
build.bat
```

The build produces:

| File | Purpose |
|---|---|
| `build/blink.elf` | Executable with symbols and debug information |
| `build/blink.bin` | ROM boot image to flash at offset `0x0` |
| `build/blink.map` | Linker memory map |
| `build/blink.lst` | Source mixed with final Xtensa instructions |

## Flash

```bat
flash.bat COM12
```

The script asks you to type `FLASH` before writing anything.

If automatic download mode fails:

1. Hold **BOOT**.
2. Press and release **RST**.
3. Release **BOOT**.
4. Run `flash.bat COMx` again.

## Read the code in this order

1. [`src/main.c`](src/main.c) — configure GPIO4, turn it on, wait, turn it off.
2. [`src/startup.S`](src/startup.S) — create the minimum environment required by C.
3. [`src/runtime.c`](src/runtime.c) — finish watchdog housekeeping normally done by a second-stage bootloader.
4. [`linker/esp32s3-baremetal.ld`](linker/esp32s3-baremetal.ld) — place code, data, and stack in internal RAM.
5. [`include/esp32s3_registers.h`](include/esp32s3_registers.h) — the small set of MMIO addresses used by the project.

