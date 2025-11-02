cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(mp3_player_native C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Generate PIO header
pico_generate_pio_header(mp3_player ${CMAKE_CURRENT_LIST_DIR}/src/i2s.pio)

# Add FatFS library path (must be cloned manually)
set(FATFS_DIR ${CMAKE_CURRENT_LIST_DIR}/lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)

# Check if FatFS exists
if(NOT EXISTS ${FATFS_DIR})
    message(FATAL_ERROR "\nno-OS-FatFS library not found!\n"
                        "Please clone it:\n"
                        "  cd ${CMAKE_CURRENT_LIST_DIR}/lib\n"
                        "  git clone https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.git\n")
endif()

# FatFS sources
add_library(FatFs_SPI INTERFACE)
target_sources(FatFs_SPI INTERFACE
    ${FATFS_DIR}/src/ff.c
    ${FATFS_DIR}/src/ffsystem.c
    ${FATFS_DIR}/src/ffunicode.c
)
target_include_directories(FatFs_SPI INTERFACE
    ${FATFS_DIR}/src
)

# no-OS-FatFS SDIO driver
add_library(no_OS_FatFS INTERFACE)
target_sources(no_OS_FatFS INTERFACE
    ${FATFS_DIR}/src/sd_driver/SDIO/SdioCard.c
    ${FATFS_DIR}/src/sd_driver/SDIO/rp2040_sdio.pio.c
    ${FATFS_DIR}/src/sd_driver/sd_card.c
    ${FATFS_DIR}/src/sd_driver/my_debug.c
)
target_include_directories(no_OS_FatFS INTERFACE
    ${FATFS_DIR}/src/sd_driver
    ${FATFS_DIR}/src/sd_driver/SDIO
)

# Main executable
add_executable(mp3_player
    src/main.c
    src/audio_i2s.c
    src/mp3_decoder.c
    src/hw_config.c
)

# Link libraries
target_link_libraries(mp3_player
    pico_stdlib
    pico_multicore
    pico_sync
    hardware_dma
    hardware_pio
    hardware_i2c
    hardware_spi
    hardware_pwm
    hardware_clocks
    hardware_irq
    FatFs_SPI
    no_OS_FatFS
)

# Include directories
target_include_directories(mp3_player PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# Enable USB output, disable UART output
pico_enable_stdio_usb(mp3_player 1)
pico_enable_stdio_uart(mp3_player 0)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(mp3_player)

# Compiler optimizations
target_compile_options(mp3_player PRIVATE
    -O3
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
)

# Linker optimizations
target_link_options(mp3_player PRIVATE
    -Wl,--print-memory-usage
)
