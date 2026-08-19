@echo off
setlocal EnableExtensions

set "PROJECT_DIR=%~dp0"
pushd "%PROJECT_DIR%" || exit /b 1

if not exist build\blink.bin (
    call build.bat
    if errorlevel 1 goto :fail
)

if not defined IDF_PATH (
    for %%I in ("%PROJECT_DIR%..\..\..\..\Tools\esp-idf") do set "IDF_PATH=%%~fI"
)

if not exist "%IDF_PATH%\export.bat" (
    echo ERROR: ESP-IDF export.bat was not found. See INSTALL.md.
    goto :fail
)

call "%IDF_PATH%\export.bat"
if errorlevel 1 goto :fail

echo.
echo WARNING: This writes a bare-metal image at flash offset 0x0.
echo The previous bootloader will not run until it is flashed again.
@REM set /p "ANSWER=Type FLASH to continue: "
@REM if /I not "%ANSWER%"=="FLASH" (
@REM     echo Flash cancelled.
@REM     popd
@REM     exit /b 0
@REM )

set "PORT_OPTION="
if not "%~1"=="" set "PORT_OPTION=--port %~1"

python.exe "%IDF_PATH%\components\esptool_py\esptool\esptool.py" --chip esp32s3 %PORT_OPTION% --before default_reset --after hard_reset write_flash --flash_mode dio --flash_freq 40m --flash_size detect 0x0 build\blink.bin
if errorlevel 1 goto :flash_fail

echo Flash completed. The external LED should now blink.
popd
exit /b 0

:flash_fail
echo Flash failed. Enter download mode and try again.
:fail
popd
exit /b 1
