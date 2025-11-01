// SD Card WAV Player - DUAL CORE VERSION
// VERSION: 1.4 (optimized for 8+ simultaneous tracks)
// DATE: 2025-11-01
// STATUS: EXPERIMENTAL - May have buffer underruns with 4+ tracks
//
// ⚠️ NOTE: If you experience distorted audio with multiple tracks,
// use SDCardWavPlayerMemory instead (pre-loads files to PSRAM for perfect playback)
//
// Uses Core1 for ALL SD operations, Core0 for AUDIO ONLY
// ZERO audio blocking - complete separation
//
// Optimizations for multi-file playback:
// - Core1 runs at maximum speed (no delay) to keep buffers full
// - Aggressive buffer filling (refills at 50% threshold)
// - Larger read chunks (2KB per read) for better SD performance
// - Volume gain tuned for mixing (0.3 = 30% per channel)
//
// RP2350B Dual ARM Cortex-M33 Architecture:
// - Core0: Audio processing only (AudioPlayQueue, mixers, I2S)
// - Core1: SD card operations only (file reading, buffering)
// - Communication: Thread-safe circular buffers with mutex
//
// SD Card Wiring (SPI):
// - SCK (Clock)  -> GP6
// - MOSI (Data In) -> GP7
// - MISO (Data Out) -> GP4
// - CS (Chip Select) -> GP5
//
// This eliminates ALL SD blocking from audio path!
//
// Controls via Serial:
// - '1'-'9','0' : Play track 1-10
// - 's' : Stop all tracks
// - 'l' : List available WAV files
// - 'd' : Show debug info

#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <SD.h>
#include <pico-audio.h>
#include <pico/multicore.h>
#include <pico/mutex.h>

// SD Card pin configuration
#define SD_CS_PIN   5
#define SD_SCK_PIN  6
#define SD_MOSI_PIN 7
#define SD_MISO_PIN 4

// Number of simultaneous players
#define NUM_PLAYERS 10

// Buffer size per player - can be smaller now since no blocking
#define BUFFER_SIZE 8192  // 8K samples = 16KB = 186ms audio

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
  volatile uint32_t core1ReadCount;

  // Mutex for this player
  mutex_t mutex;
};

// Audio components - Core0 only
WavPlayer players[NUM_PLAYERS];
AudioMixer4 mixer1, mixer2, mixer3;
AudioOutputI2S i2s1;

// Audio connections
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
// Volume: 0.0 to 1.0, divided among players
// For 10 players: 0.3 per channel = reasonable mix level
float globalVolume = 0.3;
volatile bool core1Running = false;
volatile bool sdInitialized = false;

// Forward declarations
void core1_main();
void playTrack(int playerIndex);
void stopPlayer(int playerIndex);

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
  Serial.println("║  SD WAV Player - DUAL CORE VERSION    ║");
  Serial.println("║  VERSION 1.4 (2025-11-01)             ║");
  Serial.println("║  RP2350B - Optimized for 8+ tracks    ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  Serial.println("Core0: Audio processing");
  Serial.println("Core1: SD card operations");
  Serial.println();

  // Allocate audio memory
  AudioMemory(120);

  // Configure mixers
  updateMixerGains();

  // Initialize players
  Serial.println("Initializing players...");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    mutex_init(&players[i].mutex);

    players[i].playing = false;
    players[i].stopRequested = false;
    players[i].dataSize = 0;
    players[i].dataPosition = 0;
    players[i].numChannels = 1;
    players[i].bufferSize = BUFFER_SIZE;
    players[i].bufferReadPos = 0;
    players[i].bufferWritePos = 0;
    players[i].bufferAvailable = 0;
    players[i].underrunCount = 0;
    players[i].core1ReadCount = 0;
    snprintf(players[i].filename, 32, "track%d.wav", i + 1);

    // Allocate buffer
    players[i].buffer = (int16_t*)malloc(BUFFER_SIZE * sizeof(int16_t));
    if (players[i].buffer == NULL) {
      Serial.print("ERROR: Failed to allocate buffer for player ");
      Serial.println(i + 1);
    }
  }

  Serial.print("Total buffer memory: ");
  Serial.print((NUM_PLAYERS * BUFFER_SIZE * 2) / 1024);
  Serial.println(" KB");

  // Start I2S on Core0
  i2s1.begin();

  // Launch Core1 for SD operations
  Serial.println("\nLaunching Core1 for SD operations...");
  multicore_launch_core1(core1_main);

  // Wait for Core1 to initialize SD
  Serial.print("Waiting for SD initialization on Core1");
  for (int i = 0; i < 50 && !sdInitialized; i++) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (!sdInitialized) {
    Serial.println("ERROR: Core1 failed to initialize SD card!");
    Serial.println("Check SD card and wiring.");
  } else {
    Serial.println("✓ Core1 initialized successfully");
  }

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  Ready for playback!                  ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println("\nCommands:");
  Serial.println("  1-9, 0 : Play track 1-10");
  Serial.println("  s      : Stop all");
  Serial.println("  l      : List WAV files");
  Serial.println("  d      : Debug info");
  Serial.println();
}

void loop() {
  // Handle serial commands on Core0
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd >= '1' && cmd <= '9') {
      playTrack(cmd - '1');
    } else if (cmd == '0') {
      playTrack(9);
    } else if (cmd == 's' || cmd == 'S') {
      stopAll();
    } else if (cmd == 'l' || cmd == 'L') {
      // Core1 will handle this
      Serial.println("LIST");
    } else if (cmd == 'd' || cmd == 'D') {
      showDebugInfo();
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
      Serial.print("♪ Players: ");
      Serial.print(activePlayers);
      Serial.print(" | CPU: ");
      Serial.print(AudioProcessorUsageMax());
      Serial.print("% | Mem: ");
      Serial.print(AudioMemoryUsageMax());
      Serial.print(" | Buf: ");
      Serial.print((minBuffer * 100) / BUFFER_SIZE);
      Serial.println("%");
      AudioProcessorUsageMaxReset();
    }
  }

  // CRITICAL: Small delay to prevent Core0 from starving audio system
  // Without this, Core0 loops too fast and starves the audio interrupt
  // 1ms delay is sufficient (same as Core1 delay)
  delay(1);
}

void serviceAudioQueue(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Check if queue needs data
  int16_t* queueBuffer = player->queue.getBuffer();
  if (queueBuffer == NULL) {
    return;  // Queue is full
  }

  // Lock mutex to read from shared buffer
  mutex_enter_blocking(&player->mutex);

  if (player->bufferAvailable < 128) {
    // Underrun - send silence
    mutex_exit(&player->mutex);
    memset(queueBuffer, 0, 128 * sizeof(int16_t));
    player->queue.playBuffer();
    player->underrunCount++;
    return;
  }

  // Copy from circular buffer
  for (int i = 0; i < 128; i++) {
    queueBuffer[i] = player->buffer[player->bufferReadPos];
    player->bufferReadPos = (player->bufferReadPos + 1) % BUFFER_SIZE;
  }
  player->bufferAvailable -= 128;

  mutex_exit(&player->mutex);

  player->queue.playBuffer();
}

void playTrack(int playerIndex) {
  if (playerIndex < 0 || playerIndex >= NUM_PLAYERS) return;

  WavPlayer* player = &players[playerIndex];

  if (player->playing) {
    stopPlayer(playerIndex);
    delay(50);  // Give Core1 time to close file
  }

  Serial.print("▶ Loading track ");
  Serial.println(playerIndex + 1);

  // Signal Core1 to start this player
  mutex_enter_blocking(&player->mutex);
  player->playing = true;
  player->stopRequested = false;
  player->bufferReadPos = 0;
  player->bufferWritePos = 0;
  player->bufferAvailable = 0;
  player->underrunCount = 0;
  player->core1ReadCount = 0;
  mutex_exit(&player->mutex);
}

void stopPlayer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  Serial.print("■ Stopping player ");
  Serial.println(playerIndex + 1);

  mutex_enter_blocking(&player->mutex);
  player->stopRequested = true;
  mutex_exit(&player->mutex);

  // Wait for Core1 to actually stop
  for (int i = 0; i < 100 && player->playing; i++) {
    delay(10);
  }
}

void stopAll() {
  Serial.println("■ Stopping all players...");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (players[i].playing) {
      stopPlayer(i);
    }
  }
}

void updateMixerGains() {
  // Apply globalVolume directly to each mixer channel
  // Default is 0.3 (30%) per channel for balanced multi-file playback
  for (int i = 0; i < 4; i++) {
    mixer1.gain(i, globalVolume);
    mixer2.gain(i, globalVolume);
    mixer3.gain(i, globalVolume);
  }
}

void showDebugInfo() {
  Serial.println("\n╔════════════════ DEBUG INFO ════════════════╗");
  Serial.print("║ Core1 Running: ");
  Serial.println(core1Running ? "YES" : "NO");
  Serial.print("║ SD Initialized: ");
  Serial.println(sdInitialized ? "YES" : "NO");
  Serial.println("╠════════════════════════════════════════════╣");

  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (players[i].playing) {
      Serial.print("║ Player ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(players[i].filename);
      Serial.println();
      Serial.print("║   Buffer: ");
      Serial.print((players[i].bufferAvailable * 100) / BUFFER_SIZE);
      Serial.print("% (");
      Serial.print(players[i].bufferAvailable);
      Serial.print("/");
      Serial.print(BUFFER_SIZE);
      Serial.println(")");
      Serial.print("║   Position: ");
      Serial.print(players[i].dataPosition / 1024);
      Serial.print(" / ");
      Serial.print(players[i].dataSize / 1024);
      Serial.println(" KB");
      Serial.print("║   Underruns: ");
      Serial.println(players[i].underrunCount);
      Serial.print("║   Core1 Reads: ");
      Serial.println(players[i].core1ReadCount);
    }
  }

  Serial.println("╚════════════════════════════════════════════╝\n");
}

//=============================================================================
// CORE1 FUNCTIONS - SD operations only
//=============================================================================

void core1_main() {
  core1Running = true;

  // Configure SPI for SD on Core1
  SPI.setRX(SD_MISO_PIN);
  SPI.setTX(SD_MOSI_PIN);
  SPI.setSCK(SD_SCK_PIN);

  // Initialize SD card on Core1
  if (SD.begin(SD_CS_PIN)) {
    sdInitialized = true;
  } else {
    sdInitialized = false;
    while (1) delay(1000);  // Hang if SD fails
  }

  // Main loop for Core1
  while (true) {
    // Service all players - read from SD and fill buffers
    // NO DELAY - Core1 must run as fast as possible to keep buffers full
    // With 8+ players, any delay causes buffer underruns
    for (int i = 0; i < NUM_PLAYERS; i++) {
      core1_servicePlayer(i);
    }
  }
}

void core1_servicePlayer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Check if we should start this player
  if (player->playing && !player->file && !player->stopRequested) {
    core1_openFile(playerIndex);
  }

  // Check if stop requested
  if (player->stopRequested && player->file) {
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    player->stopRequested = false;
    mutex_exit(&player->mutex);
    return;
  }

  // Fill buffer if needed
  if (player->playing && player->file) {
    core1_fillBuffer(playerIndex);
  }
}

void core1_openFile(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Open file
  player->file = SD.open(player->filename);
  if (!player->file) {
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    mutex_exit(&player->mutex);
    return;
  }

  // Read WAV header
  WavHeader header;
  player->file.read((uint8_t*)&header, sizeof(WavHeader));

  if (strncmp(header.riff, "RIFF", 4) != 0 ||
      strncmp(header.wave, "WAVE", 4) != 0 ||
      header.audioFormat != 1 ||
      header.bitsPerSample != 16) {
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    mutex_exit(&player->mutex);
    return;
  }

  // Find data chunk
  char chunkID[4];
  uint32_t chunkSize;
  bool foundData = false;

  while (player->file.available()) {
    player->file.read((uint8_t*)chunkID, 4);
    player->file.read((uint8_t*)&chunkSize, 4);

    if (strncmp(chunkID, "data", 4) == 0) {
      foundData = true;
      break;
    }
    player->file.seek(player->file.position() + chunkSize);
  }

  if (!foundData) {
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    mutex_exit(&player->mutex);
    return;
  }

  // Initialize player
  mutex_enter_blocking(&player->mutex);
  player->dataSize = chunkSize;
  player->dataPosition = 0;
  player->numChannels = header.numChannels;
  mutex_exit(&player->mutex);

  // Pre-fill buffer
  for (int i = 0; i < 4; i++) {
    core1_fillBuffer(playerIndex);
  }
}

void core1_fillBuffer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Check if buffer needs filling
  mutex_enter_blocking(&player->mutex);
  uint32_t available = player->bufferAvailable;
  uint32_t writePos = player->bufferWritePos;
  mutex_exit(&player->mutex);

  // Fill buffer when less than 50% full (more aggressive for multi-file playback)
  if (available > (BUFFER_SIZE / 2)) {
    return;  // Buffer still half full or more
  }

  // Read chunk
  uint32_t spaceAvailable = BUFFER_SIZE - available;
  uint32_t bytesRemaining = player->dataSize - player->dataPosition;

  if (bytesRemaining == 0) {
    // End of file
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    mutex_exit(&player->mutex);
    return;
  }

  // Read larger chunks (2KB instead of 1KB) for better performance
  uint32_t samplesToRead = min(spaceAvailable, (uint32_t)2048);
  uint32_t bytesToRead = min(samplesToRead * 2, bytesRemaining);
  samplesToRead = bytesToRead / 2;

  // Read without holding mutex (only this core writes to buffer)
  if (player->numChannels == 1) {
    // Mono
    for (uint32_t i = 0; i < samplesToRead; i++) {
      int16_t sample;
      player->file.read((uint8_t*)&sample, 2);
      player->buffer[writePos] = sample;
      writePos = (writePos + 1) % BUFFER_SIZE;
    }
    player->dataPosition += bytesToRead;
  } else {
    // Stereo - mix to mono
    for (uint32_t i = 0; i < samplesToRead; i++) {
      int16_t left, right;
      player->file.read((uint8_t*)&left, 2);
      player->file.read((uint8_t*)&right, 2);
      player->buffer[writePos] = ((int32_t)left + (int32_t)right) / 2;
      writePos = (writePos + 1) % BUFFER_SIZE;
    }
    player->dataPosition += (bytesToRead * 2);
  }

  // Update shared state with mutex
  mutex_enter_blocking(&player->mutex);
  player->bufferWritePos = writePos;
  player->bufferAvailable += samplesToRead;
  player->core1ReadCount++;
  mutex_exit(&player->mutex);
}
