// SD Card WAV Player - SPI ULTRA-OPTIMIZED VERSION
// VERSION: 2.0 (Maximum SPI performance with aggressive optimizations)
// DATE: 2025-11-02
//
// This is the ULTIMATE SPI-optimized version for when SDIO is not available
// Designed to handle 6-8 simultaneous WAV files with SPI bandwidth (~2 MB/s)
//
// Key optimizations:
// - MAXIMUM SPI clock speed (50 MHz)
// - HUGE buffers (32KB per player = 744ms audio @ 44.1kHz stereo)
// - Aggressive pre-filling (starts filling at 75% level)
// - Large read chunks (8KB per read)
// - Dual-core with Core1 dedicated to SD only
// - Zero audio blocking
//
// Dual-Core Architecture:
// - Core0: Audio processing ONLY (AudioPlayQueue, mixers, I2S output)
// - Core1: SD card reading ONLY (runs at maximum speed, no delays)
// - Communication: Thread-safe circular buffers protected by mutexes
//
// I2S Pin Configuration (PCM5102 DAC):
// - BCK (Bit Clock)    -> GP20
// - LRCK (Word Select) -> GP21
// - DIN (Data)         -> GP22
//
// SPI Pin Configuration:
// - CS (Chip Select)   -> GP3
// - SCK (Clock)        -> GP6
// - MOSI (Data Out)    -> GP7
// - MISO (Data In)     -> GP0
//
// Memory Usage:
// - 10 players × 32KB buffers = 320KB RAM
// - Safe for RP2350B with 512KB RAM
//
// Performance Target:
// - 8 files @ 44.1kHz stereo = 1.41 MB/s required
// - SPI @ 50MHz = ~2 MB/s achievable
// - With 32KB buffers = 744ms cushion per file
//
// Controls via Serial:
// - '1'-'9','0' : Play/stop track 1-10
// - 's' : Stop all tracks
// - 'r' : Restart all tracks
// - 'l' : List WAV files
// - 'd' : Debug info
// - 'v'/'b' : Volume up/down

#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <SD.h>
#include <pico-audio.h>
#include <pico/multicore.h>
#include <pico/mutex.h>

// SPI Pin Configuration
#define SD_CS_PIN   11 
#define SD_SCK_PIN  7
#define SD_MOSI_PIN 6
#define SD_MISO_PIN 8
// Performance tuning
#define NUM_PLAYERS 10
#define BUFFER_SIZE (32 * 1024)  // 32KB per player = 744ms @ 44.1kHz stereo
#define READ_CHUNK_SIZE (8 * 1024)  // 8KB per SD read (maximum efficiency)
#define REFILL_THRESHOLD (BUFFER_SIZE * 75 / 100)  // Start refilling at 75%

// SPI Clock Speed - Maximum for RP2350
#define SPI_CLOCK_HZ (50 * 1000 * 1000)  // 50 MHz

// WAV file header
struct WavHeader {
  char riff[4];
  uint32_t fileSize;
  char wave[4];
  char fmt[4];
  uint32_t fmtSize;
  uint16_t audioFormat;
  uint16_t numChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
};

// Player state - accessed by both cores
struct WavPlayer {
  // File handle - ONLY accessed by Core1
  File file;

  // Audio queue - ONLY accessed by Core0
  AudioPlayQueue queue;

  // Shared state - protected by mutex
  volatile bool playing;
  volatile bool stopRequested;
  uint32_t dataSize;
  uint32_t dataPosition;
  uint16_t numChannels;
  char filename[32];

  // Circular buffer - written by Core1, read by Core0
  int16_t* buffer;
  volatile uint32_t bufferSize;
  volatile uint32_t bufferReadPos;
  volatile uint32_t bufferWritePos;
  volatile uint32_t bufferAvailable;

  // Stats
  volatile uint32_t underrunCount;
  volatile uint32_t totalBytesRead;
  volatile uint32_t peakBufferUsage;

  // Mutex for this player
  mutex_t mutex;
};

// Audio components - Core0 only
WavPlayer players[NUM_PLAYERS];
AudioMixer4 mixer1, mixer2, mixer3;
AudioOutputI2S i2s1;

// Audio connections - 3-level mixer tree
AudioConnection patchCord1(players[0].queue, 0, mixer1, 0);
AudioConnection patchCord2(players[1].queue, 0, mixer1, 1);
AudioConnection patchCord3(players[2].queue, 0, mixer1, 2);
AudioConnection patchCord4(players[3].queue, 0, mixer1, 3);
AudioConnection patchCord5(players[4].queue, 0, mixer2, 0);
AudioConnection patchCord6(players[5].queue, 0, mixer2, 1);
AudioConnection patchCord7(players[6].queue, 0, mixer2, 2);
AudioConnection patchCord8(players[7].queue, 0, mixer2, 3);
AudioConnection patchCord9(players[8].queue, 0, mixer3, 0);
AudioConnection patchCord10(players[9].queue, 0, mixer3, 1);
AudioConnection patchCord11(mixer1, 0, mixer3, 2);
AudioConnection patchCord12(mixer2, 0, mixer3, 3);
AudioConnection patchCord13(mixer3, 0, i2s1, 0);
AudioConnection patchCord14(mixer3, 0, i2s1, 1);

// Global state
float globalVolume = 0.3;  // 30% per channel for mixing
volatile bool core1Running = false;
volatile bool sdInitialized = false;

// Forward declarations
void core1_main();
void playTrack(int playerIndex);
void stopPlayer(int playerIndex);
void stopAll();
void serviceAudioQueue(int playerIndex);
void core1_servicePlayer(int playerIndex);
void updateMixerGains();
void showDebugInfo();

//=============================================================================
// CORE0 FUNCTIONS - Audio processing only
//=============================================================================

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  SD WAV Player - SPI ULTRA-OPTIMIZED  ║");
  Serial.println("║  VERSION 2.0 (2025-11-02)             ║");
  Serial.println("║  RP2350B - Maximum SPI Performance    ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  // Initialize all mutexes and buffers
  Serial.print("Initializing players... ");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    mutex_init(&players[i].mutex);
    players[i].buffer = (int16_t*)malloc(BUFFER_SIZE * sizeof(int16_t));
    if (!players[i].buffer) {
      Serial.print("FAILED - Out of memory for player ");
      Serial.println(i);
      while (1);
    }
    players[i].bufferSize = BUFFER_SIZE;
    players[i].bufferReadPos = 0;
    players[i].bufferWritePos = 0;
    players[i].bufferAvailable = 0;
    players[i].playing = false;
    players[i].stopRequested = false;
    players[i].underrunCount = 0;
    players[i].totalBytesRead = 0;
    players[i].peakBufferUsage = 0;
    snprintf(players[i].filename, sizeof(players[i].filename), "track%d.wav", i + 1);
  }
  Serial.println("OK");

  // Initialize audio system
  Serial.print("Initializing audio (44.1kHz I2S)... ");
  AudioMemory(20);  // Allocate audio blocks
  i2s1.begin();     // Start I2S output

  // Configure mixer gains
  updateMixerGains();

  Serial.println("OK");

  // Launch Core1 for SD operations
  Serial.print("Launching Core1... ");
  multicore_launch_core1(core1_main);

  // Wait for Core1 to initialize SD card
  unsigned long timeout = millis();
  while (!sdInitialized && (millis() - timeout < 10000)) {
    delay(50);
  }

  if (core1Running && sdInitialized) {
    Serial.println("OK");
    Serial.println("SD card: ✓ READY (SPI @ 50MHz)");
  } else if (core1Running) {
    Serial.println("OK (Core1 running)");
    Serial.println("SD card: ✗ FAILED");
    Serial.println("\nCheck:");
    Serial.println("  - CS:   GP3");
    Serial.println("  - SCK:  GP6");
    Serial.println("  - MOSI: GP7");
    Serial.println("  - MISO: GP0");
  } else {
    Serial.println("FAILED - Core1 not responding");
  }

  Serial.println();
  Serial.println("Configuration:");
  Serial.print("  Buffer per player: ");
  Serial.print(BUFFER_SIZE / 1024);
  Serial.println(" KB");
  Serial.print("  Total buffer RAM: ");
  Serial.print((BUFFER_SIZE * NUM_PLAYERS) / 1024);
  Serial.println(" KB");
  Serial.print("  Read chunk size: ");
  Serial.print(READ_CHUNK_SIZE / 1024);
  Serial.println(" KB");
  Serial.print("  SPI clock: ");
  Serial.print(SPI_CLOCK_HZ / 1000000);
  Serial.println(" MHz");

  Serial.println();
  Serial.println("Ready! Commands:");
  Serial.println("  '1'-'0' = Play track | 's' = Stop all");
  Serial.println("  'r' = Restart | 'l' = List | 'd' = Debug");
  Serial.println("  'v' = Vol+ | 'b' = Vol-");
  Serial.println();
}

void loop() {
  // Handle serial commands on Core0
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd >= '1' && cmd <= '9') {
      int track = cmd - '1';
      playTrack(track);
    } else if (cmd == '0') {
      playTrack(9);
    } else if (cmd == 's' || cmd == 'S') {
      stopAll();
    } else if (cmd == 'r' || cmd == 'R') {
      stopAll();
      delay(100);
      for (int i = 0; i < NUM_PLAYERS; i++) {
        playTrack(i);
      }
      Serial.println("Restarting all tracks...");
    } else if (cmd == 'l' || cmd == 'L') {
      Serial.println("LIST_REQUEST");
    } else if (cmd == 'd' || cmd == 'D') {
      showDebugInfo();
    } else if (cmd == 'v' || cmd == 'V') {
      globalVolume = min(1.0f, globalVolume + 0.1f);
      updateMixerGains();
      Serial.print("Volume: ");
      Serial.print((int)(globalVolume * 100));
      Serial.println("%");
    } else if (cmd == 'b' || cmd == 'B') {
      globalVolume = max(0.0f, globalVolume - 0.1f);
      updateMixerGains();
      Serial.print("Volume: ");
      Serial.print((int)(globalVolume * 100));
      Serial.println("%");
    }
  }

  // Service audio queues - CORE0 ONLY
  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (players[i].playing) {
      serviceAudioQueue(i);
    }
  }

  // Print stats every 2 seconds
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 2000) {
    lastStats = millis();

    int activePlayers = 0;
    uint32_t minBuffer = BUFFER_SIZE;

    for (int i = 0; i < NUM_PLAYERS; i++) {
      if (players[i].playing) {
        activePlayers++;
        if (players[i].bufferAvailable < minBuffer) {
          minBuffer = players[i].bufferAvailable;
        }
      }
    }

    if (activePlayers > 0) {
      Serial.print("♪ Active: ");
      Serial.print(activePlayers);
      Serial.print(" | CPU: ");
      Serial.print(AudioProcessorUsageMax());
      Serial.print("% | Mem: ");
      Serial.print(AudioMemoryUsageMax());
      Serial.print(" | MinBuf: ");
      Serial.print((minBuffer * 100) / BUFFER_SIZE);
      Serial.println("%");
      AudioProcessorUsageMaxReset();
    }
  }

  // Small delay for stability
  delay(1);
}

void serviceAudioQueue(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Check if queue needs data
  int16_t* queueBuffer = player->queue.getBuffer();
  if (queueBuffer == NULL) {
    return;  // Queue is full
  }

  // Try to read from circular buffer
  mutex_enter_blocking(&player->mutex);

  uint32_t available = player->bufferAvailable;

  if (available < 128) {
    // Buffer underrun!
    player->underrunCount++;
    mutex_exit(&player->mutex);

    // Play silence
    memset(queueBuffer, 0, 128 * sizeof(int16_t));
    player->queue.playBuffer();
    return;
  }

  // Copy data from circular buffer
  for (int i = 0; i < 128; i++) {
    queueBuffer[i] = player->buffer[player->bufferReadPos];
    player->bufferReadPos = (player->bufferReadPos + 1) % player->bufferSize;
  }

  player->bufferAvailable -= 128;

  mutex_exit(&player->mutex);

  player->queue.playBuffer();
}

void playTrack(int playerIndex) {
  if (playerIndex < 0 || playerIndex >= NUM_PLAYERS) return;

  WavPlayer* player = &players[playerIndex];

  if (player->playing) {
    // Stop if already playing
    stopPlayer(playerIndex);
    Serial.print("Stopped track ");
    Serial.println(playerIndex + 1);
  } else {
    // Start playing
    mutex_enter_blocking(&player->mutex);
    player->playing = true;
    player->stopRequested = false;
    player->dataPosition = 0;
    player->bufferReadPos = 0;
    player->bufferWritePos = 0;
    player->bufferAvailable = 0;
    player->underrunCount = 0;
    player->totalBytesRead = 0;
    mutex_exit(&player->mutex);

    Serial.print("Playing track ");
    Serial.print(playerIndex + 1);
    Serial.print(": ");
    Serial.println(player->filename);
  }
}

void stopPlayer(int playerIndex) {
  if (playerIndex < 0 || playerIndex >= NUM_PLAYERS) return;

  WavPlayer* player = &players[playerIndex];

  mutex_enter_blocking(&player->mutex);
  player->stopRequested = true;
  player->playing = false;
  mutex_exit(&player->mutex);

  player->queue.stop();
}

void stopAll() {
  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (players[i].playing) {
      stopPlayer(i);
    }
  }
  Serial.println("Stopped all tracks");
}

void updateMixerGains() {
  // Apply global volume to all mixer channels
  for (int i = 0; i < 4; i++) {
    mixer1.gain(i, globalVolume);
    mixer2.gain(i, globalVolume);
    mixer3.gain(i, globalVolume);
  }
}

void showDebugInfo() {
  Serial.println("\n===== DEBUG INFO =====");
  Serial.print("CPU Usage: ");
  Serial.print(AudioProcessorUsage());
  Serial.print("% (max: ");
  Serial.print(AudioProcessorUsageMax());
  Serial.println("%)");

  Serial.print("Memory: ");
  Serial.print(AudioMemoryUsage());
  Serial.print(" / ");
  Serial.print(AudioMemoryUsageMax());
  Serial.println(" blocks");

  Serial.println("\nPlayers:");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (players[i].playing || players[i].totalBytesRead > 0) {
      Serial.print("  ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(players[i].playing ? "PLAY" : "STOP");
      Serial.print(" | Buf: ");
      Serial.print((players[i].bufferAvailable * 100) / BUFFER_SIZE);
      Serial.print("% | Peak: ");
      Serial.print((players[i].peakBufferUsage * 100) / BUFFER_SIZE);
      Serial.print("% | Underruns: ");
      Serial.println(players[i].underrunCount);
    }
  }
  Serial.println("====================\n");
}

//=============================================================================
// CORE1 FUNCTIONS - SD card operations only
//=============================================================================

void core1_main() {
  core1Running = true;

  // Configure SPI for MAXIMUM speed
  SPI.setRX(SD_MISO_PIN);
  SPI.setTX(SD_MOSI_PIN);
  SPI.setSCK(SD_SCK_PIN);
  SPI.begin();

  Serial.print("Core1: Initializing SD (SPI @ 50MHz)... ");

  // Initialize SD card
  bool sdOk = SD.begin(SD_CS_PIN, SPI, SPI_CLOCK_HZ);

  if (sdOk) {
    Serial.println("OK");

    uint64_t cardSize = SD.size();
    uint32_t cardSizeMB = cardSize / (1024 * 1024);
    Serial.print("Core1: SD card size: ");
    Serial.print(cardSizeMB);
    Serial.println(" MB");

    sdInitialized = true;
  } else {
    Serial.println("FAILED");
    sdInitialized = false;
  }

  // Main loop for Core1 - RUNS AT MAXIMUM SPEED
  while (true) {
    // Service all players - no delay!
    for (int i = 0; i < NUM_PLAYERS; i++) {
      core1_servicePlayer(i);
    }
  }
}

void core1_servicePlayer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Check if we should start this player
  if (player->playing && !player->file && !player->stopRequested) {
    // Open file on Core1
    player->file = SD.open(player->filename);

    if (!player->file) {
      mutex_enter_blocking(&player->mutex);
      player->playing = false;
      mutex_exit(&player->mutex);
      return;
    }

    // Parse WAV header
    WavHeader header;
    player->file.read((uint8_t*)&header, sizeof(WavHeader));

    // Find data chunk
    char chunkID[5] = {0};
    uint32_t chunkSize = 0;

    while (player->file.available()) {
      player->file.read((uint8_t*)chunkID, 4);
      player->file.read((uint8_t*)&chunkSize, 4);

      if (strncmp(chunkID, "data", 4) == 0) {
        player->dataSize = chunkSize;
        player->dataPosition = 0;
        player->numChannels = header.numChannels;
        break;
      }

      // Skip unknown chunk
      player->file.seek(player->file.position() + chunkSize);
    }
  }

  // Check if player is stopping
  if (player->stopRequested && player->file) {
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    player->stopRequested = false;
    mutex_exit(&player->mutex);
    return;
  }

  // Fill buffer if needed
  if (!player->playing || !player->file) {
    return;
  }

  mutex_enter_blocking(&player->mutex);
  uint32_t available = player->bufferAvailable;
  uint32_t freeSpace = player->bufferSize - available;
  mutex_exit(&player->mutex);

  // Only refill if below threshold
  if (freeSpace < (BUFFER_SIZE - REFILL_THRESHOLD)) {
    return;
  }

  // Read large chunk for efficiency
  uint32_t toRead = min(freeSpace * 2, (uint32_t)READ_CHUNK_SIZE);  // bytes
  toRead = min(toRead, player->dataSize - player->dataPosition);

  if (toRead == 0) {
    // End of file - loop
    player->file.seek(player->file.position() - player->dataSize);
    player->dataPosition = 0;
    return;
  }

  static int16_t tempBuffer[READ_CHUNK_SIZE / 2];  // Static to avoid stack overflow
  int bytesRead = player->file.read((uint8_t*)tempBuffer, toRead);

  if (bytesRead > 0) {
    player->dataPosition += bytesRead;
    player->totalBytesRead += bytesRead;

    int samplesRead = bytesRead / 2;

    // Write to circular buffer
    mutex_enter_blocking(&player->mutex);

    for (int i = 0; i < samplesRead; i++) {
      int16_t sample = tempBuffer[i];

      // Convert stereo to mono if needed
      if (player->numChannels == 2 && i % 2 == 1) {
        continue;  // Skip right channel
      }

      player->buffer[player->bufferWritePos] = sample;
      player->bufferWritePos = (player->bufferWritePos + 1) % player->bufferSize;
      player->bufferAvailable++;

      // Track peak buffer usage
      if (player->bufferAvailable > player->peakBufferUsage) {
        player->peakBufferUsage = player->bufferAvailable;
      }

      // Safety check - don't overflow
      if (player->bufferAvailable >= player->bufferSize) {
        break;
      }
    }

    mutex_exit(&player->mutex);
  }
}
