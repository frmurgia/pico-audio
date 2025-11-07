cmake_minimum_required(VERSION 3.13)

# Import del Pico SDK (deve essere PRIMA del project)
include(${CMAKE_CURRENT_LIST_DIR}/pico-sdk/external/pico_sdk_import.cmake)

project(mp3_player C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Inizializza SDK
pico_sdk_init()

#
# --- EXECUTABLE PRINCIPALE ------------------------------------------
#
add_executable(mp3_player
    src/main.c
    src/audio_i2s.c
    src/hw_config.c
    src/mp3_decoder.c
)

# Genera header PIO per I2S DOPO aver creato il target mp3_player
pico_generate_pio_header(mp3_player
    ${CMAKE_CURRENT_LIST_DIR}/src/i2s.pio
)

#
# --- LIBRERIA SD / FATFS --------------------------------------------
#
# Qui facciamo entrare tutta la libreria no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.
# NON cerchiamo di cherry-pickare a mano i .c, lasciamo che il suo CMake
# includa tutto quello che serve (diskio.c, rp2040_sdio.c, my_debug.c ecc.).
#
add_subdirectory(
    ${CMAKE_CURRENT_LIST_DIR}/lib/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/src
    ${CMAKE_CURRENT_BINARY_DIR}/fatfs_build
)

# Nota: il target che quella subdir esporta (di solito si chiama uguale
# alla repo) ora è disponibile e lo possiamo linkare.

#
# --- INCLUDE PATHS PER IL NOSTRO CODICE ------------------------------
#
target_include_directories(mp3_player PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

#
# --- LINK DELLE LIBRERIE --------------------------------------------
#
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
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico   # <-- target della subdir FatFS
)

# Abilita USB come stdio, disabilita UART
pico_enable_stdio_usb(mp3_player 1)
pico_enable_stdio_uart(mp3_player 0)

# Ottimizzazioni e warning "sani"
target_compile_options(mp3_player PRIVATE
    -O3
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
)

# Stampa l'uso memoria a link
target_link_options(mp3_player PRIVATE
    -Wl,--print-memory-usage
)

# Genera .elf .bin .uf2 ecc.
pico_add_extra_outputs(mp3_player)
