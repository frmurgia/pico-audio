cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(mp3_player_native C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(mp3_player
    src/main.c
    src/audio_i2s.c
    src/mp3_decoder.c
)

# Pull in common dependencies
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
)
