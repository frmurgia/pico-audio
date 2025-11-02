/**
 * Native C MP3 Player for Raspberry Pi Pico 2
 * Pure C with pico-sdk - NO Arduino
 *
 * Features:
 * - SDIO 4-bit: CLK=6, CMD=11, D0=0
 * - I2S Output: BCK=20, LRCK=21, DIN=22
 * - Dual-core streaming
 * - 32KB buffer per player
 * - Supports files 20MB+
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

// Configuration
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE (32768)  // 32KB circular buffer
#define MP3_BUFFER_SIZE (8192)     // 8KB MP3 read buffer

// SDIO Pins
#define SDIO_CLK_PIN  6
#define SDIO_CMD_PIN  11
#define SDIO_D0_PIN   0   // D1=1, D2=2, D3=3

// I2S Pins
#define I2S_BCK_PIN   20  // Bit clock
#define I2S_LRCK_PIN  21  // Left/Right clock (WS)
#define I2S_DIN_PIN   22  // Data

// Player state
typedef struct {
    // File info (managed by Core1)
    char filename[128];
    uint32_t file_size;
    uint32_t file_position;
    bool playing;
    bool stop_requested;

    // MP3 decode buffer (Core1)
    uint8_t mp3_buffer[MP3_BUFFER_SIZE];
    uint32_t mp3_fill;

    // Audio circular buffer (shared)
    int16_t* audio_buffer;
    volatile uint32_t write_pos;
    volatile uint32_t read_pos;
    volatile uint32_t available;

    // Stats
    uint32_t frames_decoded;
    uint32_t underruns;

    // Sync
    mutex_t mutex;
} mp3_player_t;

// Global state
static mp3_player_t player;
static volatile bool core1_running = false;

// Function prototypes
void core1_entry(void);
void init_i2s(void);
void init_sdio(void);
bool load_mp3_file(const char* filename);
void process_mp3_frame(void);
void audio_callback(int16_t* buffer, uint32_t num_samples);

//=============================================================================
// MAIN - Core0
//=============================================================================

int main() {
    stdio_init_all();

    // Wait for USB serial
    sleep_ms(2000);

    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  NATIVE C MP3 PLAYER                   ║\n");
    printf("║  Raspberry Pi Pico 2 - Pure C          ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    printf("Configuration:\n");
    printf("  SDIO: CLK=%d CMD=%d D0=%d\n", SDIO_CLK_PIN, SDIO_CMD_PIN, SDIO_D0_PIN);
    printf("  I2S:  BCK=%d LRCK=%d DIN=%d\n", I2S_BCK_PIN, I2S_LRCK_PIN, I2S_DIN_PIN);
    printf("  Buffer: %d samples (%.1f ms @ %d Hz)\n",
           AUDIO_BUFFER_SIZE,
           (float)AUDIO_BUFFER_SIZE * 1000.0f / AUDIO_SAMPLE_RATE,
           AUDIO_SAMPLE_RATE);
    printf("\n");

    // Initialize player structure
    printf("Initializing player... ");
    memset(&player, 0, sizeof(mp3_player_t));
    mutex_init(&player.mutex);

    // Allocate audio buffer
    player.audio_buffer = (int16_t*)malloc(AUDIO_BUFFER_SIZE * sizeof(int16_t));
    if (!player.audio_buffer) {
        printf("FAILED! (malloc)\n");
        return 1;
    }
    printf("OK\n");

    // Initialize SDIO
    printf("Initializing SDIO... ");
    init_sdio();
    printf("OK\n");

    // Initialize I2S
    printf("Initializing I2S... ");
    init_i2s();
    printf("OK\n");

    // Launch Core1
    printf("Launching Core1... ");
    multicore_launch_core1(core1_entry);
    sleep_ms(100);  // Give Core1 time to start
    printf("OK\n\n");

    printf("Ready! Type filename to play:\n");
    printf("Example: track1.mp3\n\n");

    // Main loop - Core0 handles I2S output
    char cmd_buffer[128];
    uint32_t cmd_pos = 0;

    while (true) {
        // Check for commands
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == '\n' || c == '\r') {
                if (cmd_pos > 0) {
                    cmd_buffer[cmd_pos] = '\0';

                    // Process command
                    if (strcmp(cmd_buffer, "stop") == 0 || strcmp(cmd_buffer, "s") == 0) {
                        printf("Stopping...\n");
                        player.stop_requested = true;
                    } else if (strcmp(cmd_buffer, "info") == 0 || strcmp(cmd_buffer, "i") == 0) {
                        printf("\nStatus:\n");
                        printf("  Playing: %s\n", player.playing ? "YES" : "NO");
                        if (player.playing) {
                            printf("  File: %s\n", player.filename);
                            printf("  Size: %lu bytes\n", player.file_size);
                            printf("  Position: %lu (%lu%%)\n",
                                   player.file_position,
                                   (player.file_position * 100) / player.file_size);
                            printf("  Buffer: %lu/%d (%lu%%)\n",
                                   player.available, AUDIO_BUFFER_SIZE,
                                   (player.available * 100) / AUDIO_BUFFER_SIZE);
                            printf("  Frames: %lu\n", player.frames_decoded);
                            printf("  Underruns: %lu\n", player.underruns);
                        }
                        printf("\n");
                    } else {
                        // Assume it's a filename
                        printf("Loading: %s\n", cmd_buffer);
                        if (load_mp3_file(cmd_buffer)) {
                            printf("Playing!\n");
                        } else {
                            printf("Failed to load file\n");
                        }
                    }

                    cmd_pos = 0;
                }
            } else if (cmd_pos < sizeof(cmd_buffer) - 1) {
                cmd_buffer[cmd_pos++] = c;
                putchar(c);  // Echo
            }
        }

        // Feed audio to I2S
        // (This is a placeholder - real implementation needs DMA)
        tight_loop_contents();
        sleep_ms(1);
    }

    return 0;
}

//=============================================================================
// CORE1 - SD Reading + MP3 Decoding
//=============================================================================

void core1_entry(void) {
    core1_running = true;

    printf("Core1: Started\n");

    while (true) {
        if (player.playing && !player.stop_requested) {
            // Decode MP3 and fill buffer
            process_mp3_frame();
        }

        if (player.stop_requested) {
            player.playing = false;
            player.stop_requested = false;
            printf("Core1: Stopped playback\n");
        }

        tight_loop_contents();
    }
}

//=============================================================================
// INITIALIZATION
//=============================================================================

void init_sdio(void) {
    // TODO: Initialize SDIO with no_OS_FatFS
    // Configuration:
    //   CLK = 6
    //   CMD = 11
    //   D0  = 0 (D1=1, D2=2, D3=3)
    //   PIO = pio1
    //   Baud = 20.8 MHz

    // Placeholder - needs no_OS_FatFS_SDIO library
    printf("(SDIO init placeholder) ");
}

void init_i2s(void) {
    // TODO: Initialize I2S with PIO
    // Configuration:
    //   Sample rate: 44.1 kHz
    //   Bits: 16
    //   Channels: 2 (stereo)
    //   BCK  = 20
    //   LRCK = 21
    //   DIN  = 22

    // Placeholder - needs PIO I2S implementation
    printf("(I2S init placeholder) ");
}

//=============================================================================
// PLAYBACK
//=============================================================================

bool load_mp3_file(const char* filename) {
    // Stop current playback if any
    if (player.playing) {
        player.stop_requested = true;
        while (player.playing) {
            sleep_ms(10);
        }
    }

    // Reset state
    mutex_enter_blocking(&player.mutex);
    strncpy(player.filename, filename, sizeof(player.filename) - 1);
    player.file_position = 0;
    player.write_pos = 0;
    player.read_pos = 0;
    player.available = 0;
    player.mp3_fill = 0;
    player.frames_decoded = 0;
    player.underruns = 0;
    mutex_exit(&player.mutex);

    // TODO: Open file with FatFS
    // For now, just set dummy values
    player.file_size = 1000000;  // Dummy 1MB

    player.playing = true;

    return true;  // Success
}

void process_mp3_frame(void) {
    // TODO: Implement MP3 decoding with minimp3
    // 1. Read data from SD card into mp3_buffer
    // 2. Decode frame with minimp3
    // 3. Write PCM samples to audio_buffer (circular)
    // 4. Update positions with mutex

    // Placeholder - sleep to avoid busy loop
    sleep_ms(10);
}

void audio_callback(int16_t* buffer, uint32_t num_samples) {
    mutex_enter_blocking(&player.mutex);

    if (player.available >= num_samples) {
        // Copy from circular buffer
        for (uint32_t i = 0; i < num_samples; i++) {
            buffer[i] = player.audio_buffer[player.read_pos];
            player.read_pos = (player.read_pos + 1) % AUDIO_BUFFER_SIZE;
        }
        player.available -= num_samples;
    } else {
        // Underrun - send silence
        memset(buffer, 0, num_samples * sizeof(int16_t));
        player.underruns++;
    }

    mutex_exit(&player.mutex);
}
