#!/usr/bin/env bash
# ==============================================================================
# Demonstration Script: 4 Stages of C Compilation with GCC
# ==============================================================================

CC=${CC:-gcc}

echo "[*] Using Compiler: $CC"
echo ""

# STAGE 1: Preprocessing (-E)
echo "[Stage 1/4] Preprocessing (-E)..."
$CC -E main.c -o main.i
$CC -E math.c -o math.i
echo " - Created preprocessed files: main.i, math.i"
echo ""

# STAGE 2: Compilation (-S)
echo "[Stage 2/4] Compilation to Assembly (-S)..."
$CC -S main.i -o main.s
$CC -S math.i -o math.s
echo " - Created assembly files: main.s, math.s"
echo ""

# STAGE 3: Assembly (-c)
echo "[Stage 3/4] Assembly to Object Code (-c)..."
$CC -c main.s -o main.o
$CC -c math.s -o math.o
echo " - Created relocatable object files: main.o, math.o"
echo ""

# STAGE 4: Linking (-o)
echo "[Stage 4/4] Linking (-o)..."
$CC main.o math.o -o app
echo " - Linked object files into executable binary: app"
echo ""

echo "=============================================================================="
echo " BUILD PIPELINE COMPLETED SUCCESSFULLY!"
echo "=============================================================================="
