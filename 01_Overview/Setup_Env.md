# Setup Dev Environment on Windows

## 1. Prerequisites

1. **Hardware:** An ESP32-S3 board. Connect the cable to the **Native USB** port (the port wired directly to the S3's D+/D- pins, not the UART-to-USB converter port).
2. **Software:** Install the latest **Python 3** and **Git** for Windows. Make sure to check *Add Python to PATH* during the Python installer.
3. **LLVM/Clang:** Download the latest [LLVM for Windows](https://github.com/llvm/llvm-project/releases) (e.g., `LLVM-x.y.z-win64.exe`). During installation, select **Add LLVM to system PATH for all users**. This provides the necessary tools for code formatting and static analysis.

## 2. Install the Backend (ESP-IDF Toolchain)

Open Command Prompt or PowerShell and run:

```cmd
mkdir %USERPROFILE%\esp
cd %USERPROFILE%\esp
git clone -b v5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
install.bat
```

`install.bat` automatically downloads the Xtensa compiler, CMake, Ninja, and OpenOCD.

## 3. Install the Frontend (VSCode)

Keep VSCode lightweight and powerful:

1. Open Extensions (`Ctrl+Shift+X`).
2. **Required:** Find the Microsoft **C/C++** extension and select **Disable** (or **Disable for Workspace**). We avoid it because it is resource-heavy and often conflicts with ESP-IDF's complex headers.
3. Install **clangd** (by LLVM). It provides fast, accurate code completion, navigation, and static analysis.
4. Install **Cortex-Debug** (by marus25) for JTAG debugging.

## 4. Configure a New Project Workspace

When creating a new project, copy the `.vscode/` folder from this repository into your project root. It contains two pre-configured files:

- [settings.json](../.vscode/settings.json) — configures clangd to read the build's compilation database.
- [launch.json](../.vscode/launch.json) — configures Cortex-Debug for JTAG debugging over Native USB.

These files are maintained in the repo — check them for the latest configuration.

## 5. Daily Workflow

Open the integrated terminal in VSCode (`` Ctrl+` ``).

### 5.1. Initialize the Environment (once per new terminal tab)

```cmd
%USERPROFILE%\esp\esp-idf\export.bat
```

### 5.2. Building a Project

To build a project, navigate to its directory and set the chip target:

```cmd
cd 01_Overview\BlinkLED
idf.py set-target esp32s3
```

This generates the configuration files and the compilation database for `clangd`. Add dependency led_strip (a library for controlling the onboard RGB LED):

```
idf.py add-dependency "espressif/led_strip^2.4.1"
```

Above command will create a file:
```
main/idf_component.yml
```

Build the project:

```cmd
idf.py build
```

### 5.3. Route Console Output to the USB Port

```cmd
idf.py menuconfig
```

Navigate to: `Component config` → `ESP System Settings` → `Channel for console output`
Change it to: **USB Serial/JTAG Controller**
Press `S` to save, `Q` to quit.

### 5.4. Flashing (Native USB has no auto-reset)

Because the Native USB port has no automatic reset circuit, you must manually enter download mode before flashing.

**Step 1 — Enter Download Mode:**
- Hold the `BOOT` button.
- Press and release the `RST` button.
- Release the `BOOT` button.

**Step 2 — Build and Flash:**
```cmd
idf.py flash monitor
```

**Step 3 — Start Execution:**
Once flashing reaches 100% and the terminal shows `waiting for download`, press `RST` once. The firmware will start and logs will appear immediately.

## 6. JTAG Debugging

No flashing command needed for a debug session. Set a breakpoint in the code and press **F5**. Cortex-Debug connects directly to the hardware via OpenOCD using the built-in USB JTAG interface.

## 7. Code Quality and Formatting

The project uses `clang-format` for consistent code style and `clang-tidy` for static analysis.

### 7.1. Clang-Format
- **Config File:** [.clang-format](../.clang-format) in the root directory.
- **VSCode Integration:** The project is configured to **Format on Save**. You can also manually format using `Shift+Alt+F`.
- **Key Rules:**
  - Based on **LLVM** style.
  - **4-space** indentation (no tabs).
  - **Allman** brace style (braces on new lines).
  - Column limit: 120 characters.

### 7.2. Clang-Tidy
- **Config File:** [.clang-tidy](../.clang-tidy) in the root directory.
- **VSCode Integration:** `clangd` automatically runs Clang-Tidy checks in the background. Warnings and errors will appear directly in your code and the **Problems** tab (`Ctrl+Shift+M`).
- **Included Checks:**
  - `bugprone-*`: To catch potential logic errors.
  - `performance-*`: To identify inefficient code patterns.
  - `readability-*`: To ensure code follows clean-coding standards.
