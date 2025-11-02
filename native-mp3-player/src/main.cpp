/**
 * Native MP3 Player for Raspberry Pi Pico 2
 * Optimized for LARGE files (20MB+)
 *
 * Features:
 * - SDIO 4-bit mode (10-12 MB/s)
 * - Dual-core: Core0=I2S output, Core1=SD read + MP3 decode
 * - Large buffers (32KB per channel)
 * - Streaming playback (no file size limit)
 * - minimp3 decoder
 *
 * Hardware:
 * - SDIO: CLK=GP7, CMD=GP6, DAT0-3=GP8-11
 * - I2S: BCK=GP20, LRCK=GP21, DIN=GP22
 */

#include <Arduino.h>
#include <SD.h>
#include <pico-audio.h>
#include <pico/multicore.h>
#include <pico/mutex.h>
#include <hardware/dma.h>
#include <hardware/irq.h>

// minimp3 decoder
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_ONLY_SIMD 0
#include "minimp3.h"

//=============================================================================
// CONFIGURATION
//=============================================================================

// SDIO pins
#define SD_CLK_PIN  7
#define SD_CMD_PIN  6
#define SD_DAT0_PIN 8

// I2S pins (handled by pico-audio library)
// BCK=GP20, LRCK=GP21, DIN=GP22

// Buffer configuration for LARGE files
#define AUDIO_BUFFER_SIZE (32768)  // 32KB per player = ~743ms @ 44.1kHz
#define MP3_READ_BUFFER (8192)     // 8KB MP3 read buffer
#define SD_READ_CHUNK (4096)       // 4KB SD read chunk

// Number of players
#define NUM_PLAYERS 4  // Reduced to 4 for better performance with large files

//=============================================================================
// DATA STRUCTURES
//=============================================================================

typedef struct {
    // File info
    File file;
    char filename[64];
    uint32_t fileSize;
    uint32_t filePosition;
    bool playing;
    bool stopRequested;
    bool eof;

    // MP3 decoder
    mp3dec_t decoder;
    uint8_t mp3Buffer[MP3_READ_BUFFER];
    uint32_t mp3BufferFill;
    uint32_t mp3BufferRead;

    // Audio output buffer (circular)
    int16_t* audioBuffer;
    volatile uint32_t writePos;
    volatile uint32_t readPos;
    volatile uint32_t available;

    // Audio queue
    AudioPlayQueue queue;

    // Stats
    uint32_t framesDecoded;
    uint32_t samplesDecoded;
    uint32_t underruns;
    uint32_t bytesRead;

    // Info
    uint32_t sampleRate;
    uint32_t channels;
    uint32_t bitrate;

    // Mutex
    mutex_t mutex;
} MP3Player;

//=============================================================================
// GLOBAL VARIABLES
//=============================================================================

MP3Player players[NUM_PLAYERS];

// Audio output
AudioOutputI2S i2s;

// Audio connections (4 players → 2 mixers → final mixer → I2S)
AudioMixer4 mixer1, mixer2;
AudioMixer4 mixerMaster;

AudioConnection* connections[14];

// Global state
volatile bool core1Running = false;
volatile bool sdInitialized = false;
float volume = 0.25f;  // 25% volume

//=============================================================================
// FUNCTION PROTOTYPES
//=============================================================================

void core1_main();
void initPlayers();
void playFile(uint8_t playerIdx, const char* filename);
void stopPlayer(uint8_t playerIdx);
void stopAll();
void servicePlayer(uint8_t playerIdx);
bool fillAudioBufferFromMP3(uint8_t playerIdx);
bool refillMP3Buffer(uint8_t playerIdx);
void sendToAudioQueue(uint8_t playerIdx);
void printStatus();
void listFiles();

//=============================================================================
// SETUP
//=============================================================================

void setup() {
    Serial.begin(115200);

    // Wait for serial with timeout
    unsigned long start = millis();
    while (!Serial && (millis() - start < 5000)) {
        delay(10);
    }

    Serial.println("\n╔══════════════════════════════════════════╗");
    Serial.println("║ NATIVE MP3 PLAYER - LARGE FILES         ║");
    Serial.println("║ Raspberry Pi Pico 2 - SDIO + Dual Core  ║");
    Serial.println("╚══════════════════════════════════════════╝\n");

    Serial.println("Optimized for 20MB+ MP3 files");
    Serial.println("Buffer: 32KB per player");
    Serial.println("Dual-core streaming architecture\n");

    // Initialize audio system
    Serial.print("Initializing audio... ");
    AudioMemory(80);  // 80 blocks = 40KB
    Serial.println("OK");

    // Initialize players
    Serial.print("Initializing players... ");
    initPlayers();
    Serial.println("OK");

    // Create audio connections
    int connIdx = 0;
    connections[connIdx++] = new AudioConnection(players[0].queue, 0, mixer1, 0);
    connections[connIdx++] = new AudioConnection(players[1].queue, 0, mixer1, 1);
    connections[connIdx++] = new AudioConnection(players[2].queue, 0, mixer1, 2);
    connections[connIdx++] = new AudioConnection(players[3].queue, 0, mixer1, 3);

    connections[connIdx++] = new AudioConnection(mixer1, 0, mixerMaster, 0);
    connections[connIdx++] = new AudioConnection(mixer2, 0, mixerMaster, 1);

    connections[connIdx++] = new AudioConnection(mixerMaster, 0, i2s, 0);
    connections[connIdx++] = new AudioConnection(mixerMaster, 0, i2s, 1);

    // Set mixer gains
    for (int i = 0; i < 4; i++) {
        mixer1.gain(i, volume);
        mixer2.gain(i, volume);
        mixerMaster.gain(i, volume);
    }

    // Start I2S
    Serial.print("Starting I2S... ");
    i2s.begin();
    Serial.println("OK");

    // Launch Core1
    Serial.print("Launching Core1... ");
    multicore_launch_core1(core1_main);

    // Wait for Core1 SD init
    uint32_t timeout = millis();
    while (!sdInitialized && (millis() - timeout < 10000)) {
        delay(10);
    }

    if (sdInitialized) {
        Serial.println("OK");
        Serial.println("\n✓ System ready!\n");
    } else {
        Serial.println("FAILED");
        Serial.println("\n❌ SD card initialization failed");
        Serial.println("Check SDIO wiring:\n");
        Serial.println("  CLK:  GP7");
        Serial.println("  CMD:  GP6");
        Serial.println("  DAT0: GP8 (must be consecutive)");
        Serial.println("  DAT1: GP9");
        Serial.println("  DAT2: GP10");
        Serial.println("  DAT3: GP11\n");
    }

    Serial.println("Commands:");
    Serial.println("  'p <filename>' : Play MP3 file");
    Serial.println("  '1-4'          : Play player 1-4 (track1.mp3, track2.mp3, ...)");
    Serial.println("  's'            : Stop all");
    Serial.println("  'l'            : List files");
    Serial.println("  'i'            : Info/stats");
    Serial.println("  'v <0-100>'    : Set volume\n");
}

//=============================================================================
// MAIN LOOP (Core0 - Audio only)
//=============================================================================

void loop() {
    // Handle commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        if (cmd.startsWith("p ")) {
            String filename = cmd.substring(2);
            filename.trim();

            // Find free player
            int freeIdx = -1;
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (!players[i].playing) {
                    freeIdx = i;
                    break;
                }
            }

            if (freeIdx >= 0) {
                playFile(freeIdx, filename.c_str());
            } else {
                Serial.println("All players busy - stop one first");
            }

        } else if (cmd >= "1" && cmd <= "4") {
            int idx = cmd.toInt() - 1;
            char fname[32];
            snprintf(fname, sizeof(fname), "track%d.mp3", idx + 1);
            playFile(idx, fname);

        } else if (cmd == "s") {
            stopAll();

        } else if (cmd == "l") {
            listFiles();

        } else if (cmd == "i") {
            printStatus();

        } else if (cmd.startsWith("v ")) {
            int vol = cmd.substring(2).toInt();
            if (vol >= 0 && vol <= 100) {
                volume = vol / 100.0f;
                for (int i = 0; i < 4; i++) {
                    mixer1.gain(i, volume);
                    mixer2.gain(i, volume);
                    mixerMaster.gain(i, volume);
                }
                Serial.print("Volume: ");
                Serial.print(vol);
                Serial.println("%");
            }
        }
    }

    // Service audio queues (Core0 only)
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].playing) {
            sendToAudioQueue(i);
        }
    }

    // Stats every 2 seconds
    static unsigned long lastStats = 0;
    if (millis() - lastStats > 2000) {
        lastStats = millis();

        bool anyPlaying = false;
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i].playing) {
                anyPlaying = true;
                break;
            }
        }

        if (anyPlaying) {
            Serial.print("♪ ");
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i].playing) {
                    uint32_t pct = (players[i].filePosition * 100) / players[i].fileSize;
                    Serial.print("P");
                    Serial.print(i+1);
                    Serial.print(":");
                    Serial.print(pct);
                    Serial.print("% ");
                }
            }
            Serial.print("| CPU:");
            Serial.print(AudioProcessorUsageMax());
            Serial.print("% | Mem:");
            Serial.print(AudioMemoryUsageMax());
            Serial.println();
            AudioProcessorUsageMaxReset();
        }
    }

    delay(1);
}

//=============================================================================
// PLAYER INITIALIZATION
//=============================================================================

void initPlayers() {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        MP3Player* p = &players[i];

        // Init mutex
        mutex_init(&p->mutex);

        // Allocate audio buffer
        p->audioBuffer = (int16_t*)malloc(AUDIO_BUFFER_SIZE * sizeof(int16_t));
        if (!p->audioBuffer) {
            Serial.print("ERROR: Cannot allocate buffer for player ");
            Serial.println(i);
            while(1);
        }

        // Init state
        p->playing = false;
        p->stopRequested = false;
        p->eof = false;
        p->fileSize = 0;
        p->filePosition = 0;
        p->writePos = 0;
        p->readPos = 0;
        p->available = 0;
        p->mp3BufferFill = 0;
        p->mp3BufferRead = 0;
        p->framesDecoded = 0;
        p->samplesDecoded = 0;
        p->underruns = 0;
        p->bytesRead = 0;

        // Init decoder
        mp3dec_init(&p->decoder);
    }
}

//=============================================================================
// PLAYBACK CONTROL
//=============================================================================

void playFile(uint8_t idx, const char* filename) {
    if (idx >= NUM_PLAYERS) return;

    MP3Player* p = &players[idx];

    // Stop if already playing
    if (p->playing) {
        stopPlayer(idx);
        delay(100);
    }

    // Set filename
    strncpy(p->filename, filename, sizeof(p->filename) - 1);
    p->filename[sizeof(p->filename) - 1] = '\0';

    // Reset state
    mutex_enter_blocking(&p->mutex);
    p->stopRequested = false;
    p->eof = false;
    p->filePosition = 0;
    p->writePos = 0;
    p->readPos = 0;
    p->available = 0;
    p->mp3BufferFill = 0;
    p->mp3BufferRead = 0;
    p->framesDecoded = 0;
    p->samplesDecoded = 0;
    p->underruns = 0;
    p->bytesRead = 0;
    p->playing = true;  // Signal Core1
    mutex_exit(&p->mutex);

    Serial.print("▶ Loading: ");
    Serial.println(filename);
}

void stopPlayer(uint8_t idx) {
    if (idx >= NUM_PLAYERS) return;

    MP3Player* p = &players[idx];

    if (!p->playing) return;

    p->stopRequested = true;

    // Wait for Core1 to stop
    uint32_t timeout = millis();
    while (p->playing && (millis() - timeout < 2000)) {
        delay(10);
    }

    Serial.print("■ Stopped player ");
    Serial.println(idx + 1);
}

void stopAll() {
    Serial.println("■ Stopping all");
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i].playing) {
            stopPlayer(i);
        }
    }
}

//=============================================================================
// AUDIO OUTPUT (Core0)
//=============================================================================

void sendToAudioQueue(uint8_t idx) {
    MP3Player* p = &players[idx];

    int16_t* buf = p->queue.getBuffer();
    if (!buf) return;  // Queue full

    mutex_enter_blocking(&p->mutex);

    if (p->available < 128) {
        // Underrun
        mutex_exit(&p->mutex);
        memset(buf, 0, 128 * sizeof(int16_t));
        p->queue.playBuffer();
        p->underruns++;
        return;
    }

    // Copy from circular buffer
    for (int i = 0; i < 128; i++) {
        buf[i] = p->audioBuffer[p->readPos];
        p->readPos = (p->readPos + 1) % AUDIO_BUFFER_SIZE;
    }
    p->available -= 128;

    mutex_exit(&p->mutex);

    p->queue.playBuffer();
}

//=============================================================================
// CORE1 - SD READING + MP3 DECODING
//=============================================================================

void core1_main() {
    core1Running = true;

    // Initialize SD in SDIO mode
    if (SD.begin(SD_CLK_PIN, SD_CMD_PIN, SD_DAT0_PIN)) {
        sdInitialized = true;
    } else {
        sdInitialized = false;
        while(1) delay(1000);  // Halt Core1
    }

    // Main loop
    while (true) {
        // Service all players
        for (int i = 0; i < NUM_PLAYERS; i++) {
            servicePlayer(i);
        }

        // Small yield
        tight_loop_contents();
    }
}

void servicePlayer(uint8_t idx) {
    MP3Player* p = &players[idx];

    // Check if should start
    if (p->playing && !p->file && !p->stopRequested) {
        // Open file
        p->file = SD.open(p->filename, FILE_READ);
        if (!p->file) {
            Serial.print("Core1: Failed to open ");
            Serial.println(p->filename);
            mutex_enter_blocking(&p->mutex);
            p->playing = false;
            mutex_exit(&p->mutex);
            return;
        }

        p->fileSize = p->file.size();
        mp3dec_init(&p->decoder);

        Serial.print("Core1: Opened ");
        Serial.print(p->filename);
        Serial.print(" (");
        Serial.print(p->fileSize / 1024);
        Serial.println(" KB)");
    }

    // Check if should stop
    if (p->stopRequested && p->file) {
        p->file.close();
        mutex_enter_blocking(&p->mutex);
        p->playing = false;
        p->stopRequested = false;
        mutex_exit(&p->mutex);
        return;
    }

    // Fill buffer if playing
    if (p->playing && p->file) {
        // Check if buffer needs filling
        mutex_enter_blocking(&p->mutex);
        uint32_t avail = p->available;
        mutex_exit(&p->mutex);

        // Fill when buffer < 75%
        if (avail < (AUDIO_BUFFER_SIZE * 3 / 4)) {
            fillAudioBufferFromMP3(idx);
        }
    }
}

bool fillAudioBufferFromMP3(uint8_t idx) {
    MP3Player* p = &players[idx];

    // Refill MP3 buffer if needed
    if (p->mp3BufferFill < (MP3_READ_BUFFER / 2) && p->file.available()) {
        refillMP3Buffer(idx);
    }

    // Decode frame
    if (p->mp3BufferFill > 0) {
        int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
        mp3dec_frame_info_t info;

        int samples = mp3dec_decode_frame(
            &p->decoder,
            p->mp3Buffer,
            p->mp3BufferFill,
            pcm,
            &info
        );

        if (samples > 0) {
            p->framesDecoded++;

            // Store info from first frame
            if (p->framesDecoded == 1) {
                p->sampleRate = info.hz;
                p->channels = info.channels;
                p->bitrate = info.bitrate_kbps;
            }

            // Convert stereo to mono if needed
            int monoSamples = samples;
            if (info.channels == 2) {
                monoSamples = samples / 2;
                for (int i = 0; i < monoSamples; i++) {
                    pcm[i] = (pcm[i*2] + pcm[i*2+1]) / 2;
                }
            }

            // Add to circular buffer
            mutex_enter_blocking(&p->mutex);
            uint32_t writePos = p->writePos;
            for (int i = 0; i < monoSamples && p->available < AUDIO_BUFFER_SIZE; i++) {
                p->audioBuffer[writePos] = pcm[i];
                writePos = (writePos + 1) % AUDIO_BUFFER_SIZE;
                p->available++;
            }
            p->writePos = writePos;
            mutex_exit(&p->mutex);

            p->samplesDecoded += monoSamples;

            // Remove decoded bytes
            if (info.frame_bytes > 0) {
                p->mp3BufferFill -= info.frame_bytes;
                if (p->mp3BufferFill > 0) {
                    memmove(p->mp3Buffer, p->mp3Buffer + info.frame_bytes, p->mp3BufferFill);
                }
            }

            return true;

        } else {
            // Decode failed - skip byte
            if (p->mp3BufferFill > 0) {
                p->mp3BufferFill--;
                memmove(p->mp3Buffer, p->mp3Buffer + 1, p->mp3BufferFill);
            }

            // Check EOF
            if (!p->file.available() && p->mp3BufferFill < 128) {
                p->file.close();
                mutex_enter_blocking(&p->mutex);
                p->playing = false;
                p->eof = true;
                mutex_exit(&p->mutex);
                Serial.print("Core1: Finished ");
                Serial.println(p->filename);
                return false;
            }
        }
    }

    return false;
}

bool refillMP3Buffer(uint8_t idx) {
    MP3Player* p = &players[idx];

    if (!p->file.available()) return false;

    // Shift remaining data to start
    if (p->mp3BufferFill > 0) {
        memmove(p->mp3Buffer,
                p->mp3Buffer + (MP3_READ_BUFFER - p->mp3BufferFill),
                p->mp3BufferFill);
    }

    // Read more data
    uint32_t toRead = MP3_READ_BUFFER - p->mp3BufferFill;
    if (toRead > SD_READ_CHUNK) toRead = SD_READ_CHUNK;

    uint32_t bytesRead = p->file.read(p->mp3Buffer + p->mp3BufferFill, toRead);
    p->mp3BufferFill += bytesRead;
    p->filePosition += bytesRead;
    p->bytesRead += bytesRead;

    return true;
}

//=============================================================================
// UTILITIES
//=============================================================================

void printStatus() {
    Serial.println("\n╔═══ STATUS ═══╗");
    Serial.print("  Core1: ");
    Serial.println(core1Running ? "Running" : "Stopped");
    Serial.print("  SD: ");
    Serial.println(sdInitialized ? "OK" : "Failed");

    for (int i = 0; i < NUM_PLAYERS; i++) {
        MP3Player* p = &players[i];
        if (p->playing || p->eof) {
            Serial.print("\n  Player ");
            Serial.print(i + 1);
            Serial.println(":");
            Serial.print("    File: ");
            Serial.println(p->filename);
            Serial.print("    Size: ");
            Serial.print(p->fileSize / 1024);
            Serial.println(" KB");
            Serial.print("    Progress: ");
            Serial.print((p->filePosition * 100) / p->fileSize);
            Serial.println("%");
            Serial.print("    Buffer: ");
            Serial.print(p->available);
            Serial.print("/");
            Serial.print(AUDIO_BUFFER_SIZE);
            Serial.print(" (");
            Serial.print((p->available * 100) / AUDIO_BUFFER_SIZE);
            Serial.println("%)");
            Serial.print("    Frames: ");
            Serial.println(p->framesDecoded);
            Serial.print("    Underruns: ");
            Serial.println(p->underruns);
            if (p->framesDecoded > 0) {
                Serial.print("    Format: ");
                Serial.print(p->sampleRate);
                Serial.print("Hz ");
                Serial.print(p->channels);
                Serial.print("ch ");
                Serial.print(p->bitrate);
                Serial.println("kbps");
            }
        }
    }

    Serial.println();
}

void listFiles() {
    Serial.println("\n╔═══ FILES ═══╗");

    File root = SD.open("/");
    if (!root) {
        Serial.println("  Failed to open root");
        return;
    }

    while (true) {
        File entry = root.openNextFile();
        if (!entry) break;

        if (!entry.isDirectory()) {
            String name = String(entry.name());
            name.toLowerCase();

            if (name.endsWith(".mp3")) {
                Serial.print("  ");
                Serial.print(entry.name());
                Serial.print(" (");
                Serial.print(entry.size() / 1024);
                Serial.println(" KB)");
            }
        }
        entry.close();
    }
    root.close();

    Serial.println();
}
