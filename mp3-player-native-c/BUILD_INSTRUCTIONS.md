# Build Instructions - Native C MP3 Player

## Complete Build Guide for macOS

### Prerequisites

#### 1. Install Build Tools

```bash
# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install CMake
brew install cmake

# Install ARM toolchain
brew tap ArmMbed/homebrew-formulae
brew install arm-none-eabi-gcc

# Verify installation
cmake --version
arm-none-eabi-gcc --version
```

#### 2. Setup Pico SDK

You already have pico-sdk installed via PlatformIO. Set the environment variable:

```bash
# Add to your ~/.zshrc or ~/.bash_profile
export PICO_SDK_PATH=~/.platformio/packages/framework-arduinopico/pico-sdk

# Or use the full path if different
export PICO_SDK_PATH=/Users/daniele/.platformio/packages/framework-arduinopico/pico-sdk

# Apply the changes
source ~/.zshrc  # or source ~/.bash_profile
```

To verify:
```bash
echo $PICO_SDK_PATH
ls $PICO_SDK_PATH/external/pico_sdk_import.cmake
```

### Build Process

#### Option 1: Using the Build Script (Recommended)

```bash
cd /path/to/pico-audio/mp3-player-native-c

# Make sure PICO_SDK_PATH is set
export PICO_SDK_PATH=~/.platformio/packages/framework-arduinopico/pico-sdk

# Run the build script
./build.sh
```

The script will:
- Check for PICO_SDK_PATH
- Clone no-OS-FatFS library if missing
- Create build directory
- Run CMake configuration
- Build the project
- Show flashing instructions

#### Option 2: Manual Build

```bash
cd /path/to/pico-audio/mp3-player-native-c

# 1. Clone FatFS library (if not done already)
cd lib
git clone https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.git no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
cd ..

# 2. Create build directory
mkdir -p build
cd build

# 3. Configure with CMake
cmake ..

# 4. Build
make -j4
```

### Expected Output

If build succeeds, you'll see:

```
[ 95%] Building C object CMakeFiles/mp3_player.dir/src/main.c.o
[100%] Linking CXX executable mp3_player.elf
[100%] Built target mp3_player
```

The output files will be in `build/`:
- `mp3_player.elf` - ELF binary
- `mp3_player.uf2` - UF2 file for flashing
- `mp3_player.bin` - Raw binary

### Flashing to Pico

1. **Enter BOOTSEL mode:**
   - Disconnect Pico from USB
   - Hold down the BOOTSEL button
   - Connect USB while holding BOOTSEL
   - Release BOOTSEL button
   - Pico should appear as USB drive "RPI-RP2"

2. **Copy firmware:**
   ```bash
   cp build/mp3_player.uf2 /Volumes/RPI-RP2/
   ```

3. **Pico will automatically reboot** and start running the MP3 player

### Connecting Serial Monitor

To see debug output and control the player:

```bash
# Find the device
ls /dev/tty.usbmodem*

# Connect with screen
screen /dev/tty.usbmodem14201 115200

# Or use minicom
brew install minicom
minicom -D /dev/tty.usbmodem14201 -b 115200
```

To exit screen: Press `Ctrl-A` then `K` then `Y`

### Troubleshooting

#### CMake can't find pico-sdk

**Error:**
```
include could not find requested file: pico_sdk_import.cmake
```

**Fix:**
```bash
# Make sure PICO_SDK_PATH is set correctly
export PICO_SDK_PATH=~/.platformio/packages/framework-arduinopico/pico-sdk

# Verify it exists
ls $PICO_SDK_PATH/external/pico_sdk_import.cmake

# Delete build directory and try again
rm -rf build
mkdir build
cd build
cmake ..
```

#### FatFS library not found

**Error:**
```
Could not find no-OS-FatFS library
```

**Fix:**
```bash
cd mp3-player-native-c/lib
git clone https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.git no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
```

#### Compilation errors with 'uint' type

This has been fixed in the latest commits. Make sure you have the latest code:
```bash
git pull origin claude/debug-mp3-code-issue-011CUiimErJXUksQDUZ5sUUX
```

#### ARM toolchain not found

**Error:**
```
arm-none-eabi-gcc: command not found
```

**Fix:**
```bash
brew tap ArmMbed/homebrew-formulae
brew install arm-none-eabi-gcc

# Verify
which arm-none-eabi-gcc
```

### Testing the MP3 Player

1. **Prepare SD Card:**
   - Format as FAT32
   - Copy some MP3 files to root directory
   - Insert into Pico

2. **Connect Hardware:**
   - SDIO pins: CLK=7, CMD=6, DAT0=8 (DAT1-3: 9-11)
   - I2S pins: BCK=20, LRCK=21, DIN=22
   - Connect PCM5102 DAC to I2S pins

3. **Power on and connect serial:**
   ```bash
   screen /dev/tty.usbmodem14201 115200
   ```

4. **Expected output:**
   ```
   ╔════════════════════════════════════════╗
   ║  NATIVE C MP3 PLAYER                   ║
   ║  Raspberry Pi Pico 2 - Pure C          ║
   ╚════════════════════════════════════════╝

   Configuration:
     SDIO: CLK=7 CMD=6 D0=8
     I2S:  BCK=20 LRCK=21 DIN=22
     Buffer: 32768 samples (743.0 ms @ 44100 Hz)

   Initializing...
   Ready!

   Commands:
     list - Show MP3 files
     play <filename> - Play MP3
     stop - Stop playback
   ```

5. **Play a file:**
   ```
   > list
   Files on SD card:
     track1.mp3 (3456 KB)
     track2.mp3 (5678 KB)

   > play track1.mp3
   Loading: track1.mp3
   Size: 3456 KB
   Playing...
   ```

### Clean Build

If you need to start fresh:

```bash
cd mp3-player-native-c
rm -rf build
./build.sh
```

### Build Flags

To change optimization level or add debugging symbols, edit `CMakeLists.txt`:

```cmake
# For debugging
add_compile_options(-O0 -g3)

# For release (current default)
add_compile_options(-O2)
```

### Additional Resources

- [Pico SDK Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [no-OS-FatFS GitHub](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)
- [minimp3 GitHub](https://github.com/lieff/minimp3)

### Project Structure

```
mp3-player-native-c/
├── CMakeLists.txt          # Build configuration
├── build.sh                # Automated build script
├── BUILD_INSTRUCTIONS.md   # This file
├── README.md               # Project overview
├── src/
│   ├── main.c              # Main program + dual-core logic
│   ├── audio_i2s.c         # I2S implementation (PIO + DMA)
│   ├── audio_i2s.h
│   ├── i2s.pio             # PIO program for I2S timing
│   ├── mp3_decoder.c       # minimp3 wrapper
│   ├── mp3_decoder.h
│   ├── minimp3.h           # MP3 decoder library
│   ├── hw_config.c         # SDIO hardware config
│   └── hw_config.h
└── lib/
    └── no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/  # FatFS library (cloned)
```

---

**Last Updated:** 2025-11-02
**Status:** Complete implementation - ready to build and test
**Tested On:** macOS with arm-none-eabi-gcc toolchain
