@echo off
setlocal EnableExtensions

set "PROJECT_DIR=%~dp0"
pushd "%PROJECT_DIR%" || exit /b 1

if not defined IDF_PATH (
    for %%I in ("%PROJECT_DIR%..\..\..\..\Tools\esp-idf") do set "IDF_PATH=%%~fI"
)

if not exist "%IDF_PATH%\export.bat" (
    echo ERROR: ESP-IDF export.bat was not found.
    echo Set IDF_PATH and try again.
    goto :fail
)

call "%IDF_PATH%\export.bat"
if errorlevel 1 goto :fail

where xtensa-esp32s3-elf-gcc.exe >nul 2>nul
if errorlevel 1 (
    echo ERROR: xtensa-esp32s3-elf-gcc.exe is not available.
    echo See INSTALL.md.
    goto :fail
)

if not defined FLASH_SIZE set "FLASH_SIZE=16MB"
if not exist build mkdir build

set "CC=xtensa-esp32s3-elf-gcc.exe"
set "OBJDUMP=xtensa-esp32s3-elf-objdump.exe"
set "SIZE=xtensa-esp32s3-elf-size.exe"
set "CFLAGS=-Og -g3 -Wall -Wextra -Werror -ffreestanding -fno-builtin -fdata-sections -ffunction-sections -fno-asynchronous-unwind-tables -fno-unwind-tables -mlongcalls -mtext-section-literals -mabi=call0"

echo [1/6] Assemble startup.S
%CC% %CFLAGS% -x assembler-with-cpp -c src\startup.S -o build\startup.o
if errorlevel 1 goto :fail

echo [2/6] Compile runtime.c
%CC% %CFLAGS% -std=c11 -Iinclude -c src\runtime.c -o build\runtime.o
if errorlevel 1 goto :fail

echo [3/6] Compile main.c
%CC% %CFLAGS% -std=c11 -Iinclude -c src\main.c -o build\main.o
if errorlevel 1 goto :fail

echo [4/6] Link the ELF file
%CC% -nostdlib -nostartfiles -nodefaultlibs -mabi=call0 -Wl,--gc-sections -Wl,--build-id=none -Wl,-Map=build\blink.map -T linker\esp32s3-baremetal.ld build\startup.o build\runtime.o build\main.o -o build\blink.elf
if errorlevel 1 goto :fail

echo [5/6] Convert ELF to an ESP32-S3 ROM image
python.exe "%IDF_PATH%\components\esptool_py\esptool\esptool.py" --chip esp32s3 elf2image --flash_mode dio --flash_freq 40m --flash_size %FLASH_SIZE% -o build\blink.bin build\blink.elf
if errorlevel 1 goto :fail

echo [6/6] Generate inspection files
%OBJDUMP% -d -S build\blink.elf > build\blink.lst
%SIZE% -A build\blink.elf
python.exe "%IDF_PATH%\components\esptool_py\esptool\esptool.py" --chip esp32s3 image_info build\blink.bin
if errorlevel 1 goto :fail

echo.
echo Build completed:
echo   build\blink.elf
echo   build\blink.bin
echo   build\blink.map
echo   build\blink.lst
popd
exit /b 0

:fail
echo.
echo Build failed. See INSTALL.md and README.md.
popd
exit /b 1
