@echo off
rem ==============================================================================
rem Demonstration Script: 4 Stages of C Compilation with GCC
rem ==============================================================================

set CC=gcc

rem This demo builds a program for the development computer.
rem A target cross-compiler is not a valid fallback for a host executable.
where %CC% >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: Host GCC was not found in PATH.
    echo Install a Windows GCC toolchain before running this demonstration.
    exit /b 1
)

echo [*] Using Compiler: %CC%
echo.

rem ------------------------------------------------------------------------------
rem STAGE 1: Preprocessing -> main.i, math.i
rem ------------------------------------------------------------------------------
echo [Stage 1/4] Preprocessing (-E)...
%CC% -E main.c -o main.i
%CC% -E math.c -o math.i
echo  - Created preprocessed text files: main.i, math.i
echo.

rem ------------------------------------------------------------------------------
rem STAGE 2: Compilation -> main.s, math.s
rem ------------------------------------------------------------------------------
echo [Stage 2/4] Compilation to Assembly (-S)...
%CC% -S main.i -o main.s
%CC% -S math.i -o math.s
echo  - Created assembly files: main.s, math.s
echo.

rem ------------------------------------------------------------------------------
rem STAGE 3: Assembly -> main.o, math.o
rem ------------------------------------------------------------------------------
echo [Stage 3/4] Assembly to Object Code (-c)...
%CC% -c main.s -o main.o
%CC% -c math.s -o math.o
echo  - Created relocatable object files: main.o, math.o
echo.

rem ------------------------------------------------------------------------------
rem STAGE 4: Linking -> app.exe / app.elf
rem ------------------------------------------------------------------------------
echo [Stage 4/4] Linking (-o)...
%CC% main.o math.o -o app.exe
echo  - Linked object files into executable binary: app.exe
echo.

echo ==============================================================================
echo  BUILD PIPELINE COMPLETED SUCCESSFULLY!
echo ==============================================================================
