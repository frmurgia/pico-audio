/**
 * Native C MP3 Player for Raspberry Pi Pico 2
 * Complete Implementation - NO Arduino, Pure C
 *
 * Features:
 * - SDIO 4-bit: CLK=6, CMD=11, D0=0
 * - I2S Output: BCK=20, LRCK=21, DIN=22
 * - Dual-core streaming
 * - 32KB circular buffer
 * - Supports large MP3 files (20MB+)
 * - DMA-based I2S output
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/mutex.h"
#include "hardware/clocks.h"

// FatFS
#include "ff.h"

// Our modules
#include "audio_i2s.h"
#include "mp3_decoder.h"
#include "hw_config.h"

//=============================================================================
// CONFIGURATION
//=============================================================================

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_BUFFER_SIZE (32768)  // 32KB circular buffer
#define MP3_BUFFER_SIZE (8192)     // 8KB MP3 read buffer
#define MP3_READ_CHUNK (2048)      // 2KB SD read chunks

// SDIO Pins (configured in hw_config.c)
#define SDIO_CLK_PIN  7
#define SDIO_CMD_PIN  6
#define SDIO_D0_PIN   8  // D1=9, D2=10, D3=11

// I2S Pins
#define I2S_BCK_PIN   20
#define I2S_LRCK_PIN  21
#define I2S_DIN_PIN   22

//=============================================================================
// PLAYER STATE
//=============================================================================

typedef struct {
    // File info (Core1)
    FIL file;
    char filename[128];
    uint32_t file_size;
    uint32_t file_position;
    bool file_open;
    bool playing;
    bool stop_requested;
    bool eof;

    // MP3 decoder (Core1)
    mp3_decoder_t decoder;
    uint8_t mp3_buffer[MP3_BUFFER_SIZE];
    uint32_t mp3_fill;

    // Audio circular buffer (shared, protected by mutex)
    int16_t* audio_buffer;
    volatile uint32_t write_pos;
    volatile uint32_t read_pos;
    volatile uint32_t available;

    // Stats
    uint32_t frames_decoded;
    uint32_t samples_decoded;
    uint32_t underruns;
    uint32_t bytes_read;

    // Info
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bitrate;

    // Sync
    mutex_t mutex;
} mp3_player_t;

//=============================================================================
// GLOBALS
//=============================================================================

static mp3_player_t player;
static volatile bool core1_running = false;
static volatile bool system_ready = false;

//=============================================================================
// FUNCTION PROTOTYPES
//=============================================================================

void core1_entry(void);
void core1_main_loop(void);
bool load_mp3_file(const char* filename);
void stop_playback(void);
bool read_mp3_data(void);
bool decode_mp3_frame(void);
void audio_callback(int16_t* buffer, uint32_t num_samples, void* user_data);
void print_status(void);
void list_mp3_files(void);
void process_command(const char* cmd);

//=============================================================================
// MAIN - Core0
//=============================================================================

int main() {
    // Initialize stdio
    stdio_init_all();
    sleep_ms(2000);  // Wait for USB serial

    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  NATIVE C MP3 PLAYER - COMPLETE       ║\n");
    printf("║  Raspberry Pi Pico 2                   ║\n");
    printf("║  Pure C + pico-sdk                     ║\n");
    printf("╚════════════════════════════════════════╝\n\n");

    printf("Configuration:\n");
    printf("  SDIO: CLK=%d CMD=%d D0=%d\n", SDIO_CLK_PIN, SDIO_CMD_PIN, SDIO_D0_PIN);
    printf("  I2S:  BCK=%d LRCK=%d DIN=%d\n", I2S_BCK_PIN, I2S_LRCK_PIN, I2S_DIN_PIN);
    printf("  Buffer: %d samples (%.1f ms)\n",
           AUDIO_BUFFER_SIZE,
           (float)AUDIO_BUFFER_SIZE * 1000.0f / AUDIO_SAMPLE_RATE);
    printf("\n");

    // Initialize player
    printf("Initializing player... ");
    memset(&player, 0, sizeof(player));
    mutex_init(&player.mutex);

    player.audio_buffer = (int16_t*)malloc(AUDIO_BUFFER_SIZE * sizeof(int16_t));
    if (!player.audio_buffer) {
        printf("FAILED (malloc)\n");
        return 1;
    }
    memset(player.audio_buffer, 0, AUDIO_BUFFER_SIZE * sizeof(int16_t));

    mp3_decoder_init(&player.decoder);
    printf("OK\n");

    // Initialize SDIO
    printf("Initializing SDIO... ");
    if (!hw_config_init_sd()) {
        printf("FAILED\n");
        printf("\nSD card initialization failed!\n");
        printf("Check:\n");
        printf("  - SD card inserted\n");
        printf("  - Wiring: CLK=6, CMD=11, DAT0-3=0-3\n");
        printf("  - SD card formatted FAT32\n");
        return 1;
    }
    printf("OK\n");

    // Initialize I2S
    printf("Initializing I2S... ");
    audio_i2s_init(I2S_BCK_PIN, I2S_LRCK_PIN, I2S_DIN_PIN, AUDIO_SAMPLE_RATE);
    audio_i2s_set_callback(audio_callback, &player);
    printf("OK\n");

    // Launch Core1
    printf("Launching Core1... ");
    multicore_launch_core1(core1_entry);
    sleep_ms(100);

    // Wait for Core1 ready
    uint32_t timeout = time_us_32();
    while (!core1_running && (time_us_32() - timeout) < 2000000) {
        sleep_ms(10);
    }

    if (core1_running) {
        printf("OK\n");
    } else {
        printf("FAILED\n");
        return 1;
    }

    system_ready = true;
    printf("\n✓ System ready!\n\n");

    printf("Commands:\n");
    printf("  <filename>  - Play MP3 file (e.g. track1.mp3)\n");
    printf("  stop / s    - Stop playback\n");
    printf("  list / l    - List MP3 files\n");
    printf("  info / i    - Show player info\n");
    printf("  help / h    - Show this help\n\n");

    printf("> ");
    fflush(stdout);

    // Main loop - Core0
    char cmd_buffer[256];
    uint32_t cmd_pos = 0;

    while (true) {
        // Read command
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            if (c == '\n' || c == '\r') {
                if (cmd_pos > 0) {
                    cmd_buffer[cmd_pos] = '\0';
                    printf("\n");
                    process_command(cmd_buffer);
                    cmd_pos = 0;
                    printf("> ");
                    fflush(stdout);
                }
            } else if (c == '\b' || c == 127) {  // Backspace
                if (cmd_pos > 0) {
                    cmd_pos--;
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (cmd_pos < sizeof(cmd_buffer) - 1 && c >= 32 && c < 127) {
                cmd_buffer[cmd_pos++] = c;
                putchar(c);
                fflush(stdout);
            }
        }

        // Print progress every 2 seconds
        static uint32_t last_progress = 0;
        if (player.playing && (time_us_32() - last_progress) > 2000000) {
            last_progress = time_us_32();

            uint32_t pct = (player.file_position * 100) / player.file_size;
            uint32_t buf_pct = (player.available * 100) / AUDIO_BUFFER_SIZE;

            printf("\r[%3lu%%] Buf:%3lu%% Frames:%lu",
                   pct, buf_pct, player.frames_decoded);
            fflush(stdout);
        }

        tight_loop_contents();
        sleep_ms(1);
    }

    return 0;
}

//=============================================================================
// COMMAND PROCESSING
//=============================================================================

void process_command(const char* cmd) {
    if (strlen(cmd) == 0) return;

    // Check for keywords
    if (strcmp(cmd, "stop") == 0 || strcmp(cmd, "s") == 0) {
        if (player.playing) {
            printf("Stopping playback...\n");
            stop_playback();
        } else {
            printf("Not playing\n");
        }
    }
    else if (strcmp(cmd, "list") == 0 || strcmp(cmd, "l") == 0) {
        list_mp3_files();
    }
    else if (strcmp(cmd, "info") == 0 || strcmp(cmd, "i") == 0) {
        print_status();
    }
    else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
        printf("Commands:\n");
        printf("  <filename>  - Play MP3 file\n");
        printf("  stop / s    - Stop playback\n");
        printf("  list / l    - List MP3 files\n");
        printf("  info / i    - Show player info\n");
        printf("  help / h    - Show this help\n");
    }
    else {
        // Assume it's a filename
        if (load_mp3_file(cmd)) {
            printf("Playing: %s\n", cmd);
            audio_i2s_start();
        } else {
            printf("Failed to load: %s\n", cmd);
        }
    }
}

//=============================================================================
// FILE OPERATIONS
//=============================================================================

bool load_mp3_file(const char* filename) {
    // Stop current playback
    if (player.playing) {
        stop_playback();
        sleep_ms(100);
    }

    // Open file
    FRESULT fr = f_open(&player.file, filename, FA_READ);
    if (fr != FR_OK) {
        printf("Error opening file: %d\n", fr);
        return false;
    }

    player.file_size = f_size(&player.file);
    player.file_open = true;

    // Reset state
    mutex_enter_blocking(&player.mutex);
    strncpy(player.filename, filename, sizeof(player.filename) - 1);
    player.file_position = 0;
    player.write_pos = 0;
    player.read_pos = 0;
    player.available = 0;
    player.mp3_fill = 0;
    player.frames_decoded = 0;
    player.samples_decoded = 0;
    player.underruns = 0;
    player.bytes_read = 0;
    player.eof = false;
    player.playing = true;
    mutex_exit(&player.mutex);

    // Reinit decoder
    mp3_decoder_init(&player.decoder);

    printf("Opened: %s (%lu KB)\n", filename, player.file_size / 1024);

    return true;
}

void stop_playback(void) {
    player.stop_requested = true;

    // Wait for Core1 to stop
    uint32_t timeout = time_us_32();
    while (player.playing && (time_us_32() - timeout) < 2000000) {
        sleep_ms(10);
    }

    if (player.playing) {
        printf("Warning: Timeout waiting for stop\n");
        player.playing = false;
    }

    audio_i2s_stop();
    printf("Stopped\n");
}

void list_mp3_files(void) {
    printf("\nMP3 files on SD card:\n");

    DIR dir;
    FILINFO fno;

    FRESULT fr = f_opendir(&dir, "/");
    if (fr != FR_OK) {
        printf("Error opening directory\n");
        return;
    }

    int count = 0;
    while (true) {
        fr = f_readdir(&dir, &fno);
        if (fr != FR_OK || fno.fname[0] == 0) break;

        // Check if MP3
        size_t len = strlen(fno.fname);
        if (len > 4) {
            const char* ext = fno.fname + len - 4;
            if (strcasecmp(ext, ".mp3") == 0) {
                printf("  %s (%llu KB)\n", fno.fname, (unsigned long long)(fno.fsize / 1024));
                count++;
            }
        }
    }

    f_closedir(&dir);

    if (count == 0) {
        printf("  (no MP3 files found)\n");
    }
    printf("\n");
}

void print_status(void) {
    printf("\n╔═══ PLAYER STATUS ═══╗\n");
    printf("║ Playing: %s\n", player.playing ? "YES" : "NO");

    if (player.playing || player.eof) {
        printf("║ File: %s\n", player.filename);
        printf("║ Size: %lu KB\n", player.file_size / 1024);
        printf("║ Position: %lu / %lu (%lu%%)\n",
               player.file_position, player.file_size,
               (player.file_position * 100) / player.file_size);
        printf("║ Buffer: %lu / %d (%lu%%)\n",
               player.available, AUDIO_BUFFER_SIZE,
               (player.available * 100) / AUDIO_BUFFER_SIZE);
        printf("║ Frames decoded: %lu\n", player.frames_decoded);
        printf("║ Samples: %lu\n", player.samples_decoded);
        printf("║ Underruns: %lu\n", player.underruns);

        if (player.frames_decoded > 0) {
            printf("║ Format: %lu Hz, %lu ch, %lu kbps\n",
                   player.sample_rate, player.channels, player.bitrate);
        }
    }

    printf("╚═════════════════════╝\n\n");
}

//=============================================================================
// CORE1 - SD READ + MP3 DECODE
//=============================================================================

void core1_entry(void) {
    core1_running = true;
    printf("Core1: Started\n");

    // Wait for system ready
    while (!system_ready) {
        tight_loop_contents();
    }

    // Main loop
    core1_main_loop();
}

void core1_main_loop(void) {
    while (true) {
        if (player.playing && !player.stop_requested) {
            // Check if buffer needs filling
            mutex_enter_blocking(&player.mutex);
            uint32_t avail = player.available;
            mutex_exit(&player.mutex);

            // Fill buffer when < 75% full
            if (avail < (AUDIO_BUFFER_SIZE * 3 / 4)) {
                // Read more MP3 data if needed
                if (player.mp3_fill < (MP3_BUFFER_SIZE / 2)) {
                    read_mp3_data();
                }

                // Decode frame
                decode_mp3_frame();
            }
        }

        if (player.stop_requested) {
            if (player.file_open) {
                f_close(&player.file);
                player.file_open = false;
            }

            mutex_enter_blocking(&player.mutex);
            player.playing = false;
            player.stop_requested = false;
            mutex_exit(&player.mutex);
        }

        tight_loop_contents();
    }
}

bool read_mp3_data(void) {
    if (!player.file_open || player.eof) return false;

    // Calculate space available in MP3 buffer
    uint32_t space = MP3_BUFFER_SIZE - player.mp3_fill;
    if (space == 0) return false;

    // Read chunk
    uint32_t to_read = (space < MP3_READ_CHUNK) ? space : MP3_READ_CHUNK;

    UINT bytes_read;
    FRESULT fr = f_read(&player.file,
                        player.mp3_buffer + player.mp3_fill,
                        to_read,
                        &bytes_read);

    if (fr != FR_OK) {
        printf("Core1: Read error %d\n", fr);
        return false;
    }

    player.mp3_fill += bytes_read;
    player.file_position += bytes_read;
    player.bytes_read += bytes_read;

    // Check EOF
    if (bytes_read < to_read) {
        player.eof = true;
    }

    return true;
}

bool decode_mp3_frame(void) {
    if (player.mp3_fill == 0) {
        // No data to decode
        if (player.eof) {
            // End of file
            if (player.file_open) {
                f_close(&player.file);
                player.file_open = false;
            }

            mutex_enter_blocking(&player.mutex);
            player.playing = false;
            mutex_exit(&player.mutex);

            printf("\nCore1: Playback finished\n");
        }
        return false;
    }

    // Decode one frame
    int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    mp3_frame_info_t info;

    int samples = mp3_decoder_decode_frame(&player.decoder,
                                            player.mp3_buffer,
                                            player.mp3_fill,
                                            pcm,
                                            &info);

    if (samples > 0) {
        player.frames_decoded++;

        // Store format info on first frame
        if (player.frames_decoded == 1) {
            player.sample_rate = info.sample_rate;
            player.channels = info.channels;
            player.bitrate = info.bitrate_kbps;
            printf("Core1: MP3 format - %lu Hz, %lu ch, %lu kbps\n",
                   player.sample_rate, player.channels, player.bitrate);
        }

        // Convert stereo to mono if needed
        int mono_samples = samples;
        if (info.channels == 2) {
            int16_t mono[MINIMP3_MAX_SAMPLES_PER_FRAME / 2];
            mp3_decoder_convert_to_mono(pcm, samples, mono);
            mono_samples = samples / 2;

            // Copy mono to PCM buffer
            memcpy(pcm, mono, mono_samples * sizeof(int16_t));
        }

        // Write to circular buffer
        mutex_enter_blocking(&player.mutex);
        uint32_t writePos = player.write_pos;

        for (int i = 0; i < mono_samples && player.available < AUDIO_BUFFER_SIZE; i++) {
            player.audio_buffer[writePos] = pcm[i];
            writePos = (writePos + 1) % AUDIO_BUFFER_SIZE;
            player.available++;
        }

        player.write_pos = writePos;
        mutex_exit(&player.mutex);

        player.samples_decoded += mono_samples;

        // Remove decoded data from MP3 buffer
        if (info.frame_bytes > 0 && info.frame_bytes <= player.mp3_fill) {
            uint32_t remaining = player.mp3_fill - info.frame_bytes;
            if (remaining > 0) {
                memmove(player.mp3_buffer,
                        player.mp3_buffer + info.frame_bytes,
                        remaining);
            }
            player.mp3_fill = remaining;
        }

        return true;

    } else {
        // Decode failed - skip one byte for resync
        if (player.mp3_fill > 0) {
            memmove(player.mp3_buffer, player.mp3_buffer + 1, player.mp3_fill - 1);
            player.mp3_fill--;
        }

        return false;
    }
}

//=============================================================================
// AUDIO CALLBACK - Called by DMA interrupt
//=============================================================================

void audio_callback(int16_t* buffer, uint32_t num_samples, void* user_data) {
    mp3_player_t* p = (mp3_player_t*)user_data;

    if (!p->playing) {
        // Not playing - send silence
        memset(buffer, 0, num_samples * sizeof(int16_t));
        return;
    }

    mutex_enter_blocking(&p->mutex);

    if (p->available >= num_samples) {
        // Copy from circular buffer
        for (uint32_t i = 0; i < num_samples; i++) {
            buffer[i] = p->audio_buffer[p->read_pos];
            p->read_pos = (p->read_pos + 1) % AUDIO_BUFFER_SIZE;
        }
        p->available -= num_samples;
    } else {
        // Underrun - send silence
        memset(buffer, 0, num_samples * sizeof(int16_t));
        p->underruns++;
    }

    mutex_exit(&p->mutex);
}
