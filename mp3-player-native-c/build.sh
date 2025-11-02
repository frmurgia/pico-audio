#!/bin/bash
#
# Build script for Native C MP3 Player
# Simplifies the build process
#

set -e  # Exit on error

echo "╔════════════════════════════════════════╗"
echo "║  Native C MP3 Player - Build Script   ║"
echo "╚════════════════════════════════════════╝"
echo ""

# Check PICO_SDK_PATH
if [ -z "$PICO_SDK_PATH" ]; then
    echo "❌ Error: PICO_SDK_PATH not set"
    echo ""
    echo "Please set PICO_SDK_PATH:"
    echo "  export PICO_SDK_PATH=/path/to/pico-sdk"
    echo ""
    exit 1
fi

echo "✓ PICO_SDK_PATH: $PICO_SDK_PATH"

# Check if FatFS library exists
FATFS_DIR="lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico"
if [ ! -d "$FATFS_DIR" ]; then
    echo ""
    echo "❌ Error: no-OS-FatFS library not found"
    echo ""
    echo "Cloning no-OS-FatFS library..."
    mkdir -p lib
    cd lib
    git clone https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.git no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
    cd ..
    echo "✓ Library cloned"
fi

echo "✓ FatFS library found"

# Create build directory
if [ ! -d "build" ]; then
    echo ""
    echo "Creating build directory..."
    mkdir build
fi

cd build

# Configure
echo ""
echo "Configuring with CMake..."
cmake .. || {
    echo "❌ CMake configuration failed"
    exit 1
}

# Build
echo ""
echo "Building..."
make -j$(nproc 2>/dev/null || echo 4) || {
    echo "❌ Build failed"
    exit 1
}

echo ""
echo "╔════════════════════════════════════════╗"
echo "║  BUILD SUCCESSFUL!                     ║"
echo "╚════════════════════════════════════════╝"
echo ""
echo "Output: build/mp3_player.uf2"
echo ""
echo "To flash:"
echo "  1. Hold BOOTSEL button on Pico"
echo "  2. Connect USB"
echo "  3. Release BOOTSEL"
echo "  4. Copy file:"
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "     cp mp3_player.uf2 /Volumes/RPI-RP2/"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "     cp mp3_player.uf2 /media/$USER/RPI-RP2/"
else
    echo "     Copy mp3_player.uf2 to RPI-RP2 drive"
fi
echo ""
