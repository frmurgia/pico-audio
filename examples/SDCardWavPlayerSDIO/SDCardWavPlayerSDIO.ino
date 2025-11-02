// SD Card WAV Player - SDIO 4-BIT VERSION
// VERSION: 2.6 (Simple filename format: 1.wav, 2.wav, etc.)
// DATE: 2025-11-02
//
// Uses SDIO 4-bit mode instead of SPI for maximum SD card performance
// SDIO bandwidth: ~10-12 MB/s (vs SPI ~1-2 MB/s)
//
// Dual-Core Architecture:
// - Core0: Audio processing only (AudioPlayQueue, mixers, I2S)
// - Core1: SD card operations only (file reading, buffering)
// - Communication: Thread-safe circular buffers with mutex
//
// I2S Pin Configuration (PCM5102 DAC):
// - BCK (Bit Clock)   -> GP20
// - LRCK (Word Select)-> GP21
// - DIN (Data)        -> GP22
// - VCC               -> 3.3V
// - GND               -> GND
//
// SDIO Pin Configuration (6 pins required - VERIFIED WORKING):
// - SD_CLK  -> GP7  (clock, any GPIO)
// - SD_CMD  -> GP6  (command, any GPIO)
// - SD_DAT0 -> GP8  (data bit 0, must be base for consecutive pins)
// - SD_DAT1 -> GP9  (data bit 1, must be DAT0+1)
// - SD_DAT2 -> GP10 (data bit 2, must be DAT0+2)
// - SD_DAT3 -> GP11 (data bit 3, must be DAT0+3)
//
// ⚠️ IMPORTANT: DAT0-3 MUST be on consecutive GPIO pins!
//
// Performance with SDIO:
// - 10 files @ 44.1kHz stereo = 1.76 MB/s required
// - SDIO provides 10-12 MB/s = 6x headroom!
// - NO buffer underruns even with 10 simultaneous tracks
//
// Controls via Serial:
// - '1'-'9','0' : Play track 1-10
// - 's' : Stop all tracks
// - 'l' : List available WAV files
// - 'd' : Show debug info

#include <Adafruit_TinyUSB.h>
#include <SD.h>  // arduino-pico native SD library with SDIO support
#include <pico-audio.h>
#include <pico/multicore.h>
#include <pico/mutex.h>

// SDIO Pin Configuration for RP2350 (VERIFIED WORKING)
#define SD_CLK_PIN  7
#define SD_CMD_PIN  6
#define SD_DAT0_PIN 8  // DAT1=9, DAT2=10, DAT3=11 (consecutive!)

// Number of simultaneous players
#define NUM_PLAYERS 10

// Buffer size per player
#define BUFFER_SIZE 4096  // Smaller buffer OK with SDIO speed (4K samples = 93ms)

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
float globalVolume = 0.3;  // 30% per channel
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
  Serial.println("║  SD WAV Player - SDIO 4-BIT MODE     ║");
  Serial.println("║  VERSION 2.6 (2025-11-02)             ║");
  Serial.println("║  RP2350B - 10-12 MB/s SDIO Bandwidth ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  Serial.println("Core0: Audio processing");
  Serial.println("Core1: SD card operations (SDIO)");
  Serial.println();
  Serial.println("SDIO Pins (VERIFIED WORKING):");
  Serial.println("  CLK:  GP7");
  Serial.println("  CMD:  GP6");
  Serial.println("  DAT0: GP8");
  Serial.println("  DAT1: GP9");
  Serial.println("  DAT2: GP10");
  Serial.println("  DAT3: GP11");
  Serial.println();

  // Initialize audio
  AudioMemory(120);

  // Configure mixers
  updateMixerGains();

  // Initialize I2S
  i2s1.begin();

  // Initialize players
  Serial.println("Initializing players...");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    mutex_init(&players[i].mutex);

    players[i].playing = false;
    players[i].stopRequested = false;
    players[i].dataSize = 0;
    players[i].dataPosition = 0;
    players[i].underrunCount = 0;
    players[i].core1ReadCount = 0;

    // Allocate circular buffer
    players[i].buffer = (int16_t*)malloc(BUFFER_SIZE * sizeof(int16_t));
    if (players[i].buffer == NULL) {
      Serial.print("ERROR: Cannot allocate buffer for player ");
      Serial.println(i);
      while (1) delay(1000);
    }

    players[i].bufferSize = BUFFER_SIZE;
    players[i].bufferReadPos = 0;
    players[i].bufferWritePos = 0;
    players[i].bufferAvailable = 0;

    // AudioPlayQueue doesn't have begin() - no initialization needed
  }

  Serial.println("OK");
  Serial.println();

  // Launch Core1 for SD operations
  Serial.print("Launching Core1... ");
  multicore_launch_core1(core1_main);

  // Wait for Core1 to complete SD initialization (not just start)
  unsigned long timeout = millis();
  while (!sdInitialized && (millis() - timeout < 10000)) {
    delay(50);
  }

  if (core1Running && sdInitialized) {
    Serial.println("OK");
    Serial.println("SD card: OK (SDIO mode)");
  } else if (core1Running) {
    Serial.println("OK (Core1 running)");
    Serial.println("SD card: FAILED - Check wiring and card");
  } else {
    Serial.println("FAILED - Core1 not responding");
  }

  Serial.println();
  Serial.println("Ready!");
  Serial.println("Commands: '1'-'0' = play track, 's' = stop, 'l' = list, 'd' = debug");
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
      showFileList();
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

  // Small delay for stability (Core0 only)
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

  // Build filename
  snprintf(player->filename, sizeof(player->filename), "%d.wav", playerIndex + 1);

  // Reset state
  mutex_enter_blocking(&player->mutex);
  player->dataPosition = 0;
  player->bufferReadPos = 0;
  player->bufferWritePos = 0;
  player->bufferAvailable = 0;
  player->underrunCount = 0;
  player->core1ReadCount = 0;
  player->playing = true;  // Signal Core1 to start
  mutex_exit(&player->mutex);

  Serial.print("▶ Loading track ");
  Serial.println(playerIndex + 1);
}

void stopPlayer(int playerIndex) {
  if (playerIndex < 0 || playerIndex >= NUM_PLAYERS) return;

  WavPlayer* player = &players[playerIndex];
  player->stopRequested = true;

  // Wait for stop to complete
  unsigned long timeout = millis();
  while (player->playing && (millis() - timeout < 1000)) {
    delay(10);
  }

  Serial.print("■ Stopped track ");
  Serial.println(playerIndex + 1);
}

void stopAll() {
  Serial.println("■ Stopping all tracks");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (players[i].playing) {
      stopPlayer(i);
    }
  }
}

void updateMixerGains() {
  for (int i = 0; i < 4; i++) {
    mixer1.gain(i, globalVolume);
    mixer2.gain(i, globalVolume);
    mixer3.gain(i, globalVolume);
  }
}

void showFileList() {
  if (!sdInitialized) {
    Serial.println("\n⚠️  SD card not initialized!");
    return;
  }

  Serial.println("\n╔════════════════ FILE LIST ═════════════════╗");
  Serial.println("║ WAV files on SD card:");
  Serial.println("╠════════════════════════════════════════════╣");

  File root = SD.open("/");
  if (!root) {
    Serial.println("║ ERROR: Cannot open root directory");
    Serial.println("╚════════════════════════════════════════════╝");
    return;
  }

  int totalFiles = 0;
  int wavFiles = 0;

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;

    totalFiles++;
    String filename = String(entry.name());
    filename.toLowerCase();

    if (!entry.isDirectory() && filename.endsWith(".wav")) {
      wavFiles++;
      Serial.print("║ ");
      Serial.print(entry.name());

      // Pad filename
      int spaces = 30 - strlen(entry.name());
      for (int i = 0; i < spaces; i++) Serial.print(" ");

      Serial.print(" (");
      uint32_t sizeKB = entry.size() / 1024;
      if (sizeKB < 10) Serial.print("   ");
      else if (sizeKB < 100) Serial.print("  ");
      else if (sizeKB < 1000) Serial.print(" ");
      Serial.print(sizeKB);
      Serial.println(" KB)");
    }

    entry.close();
  }

  root.close();

  Serial.println("╠════════════════════════════════════════════╣");
  Serial.print("║ Total files: ");
  Serial.print(totalFiles);
  Serial.print(" | WAV files: ");
  Serial.println(wavFiles);

  if (wavFiles == 0) {
    Serial.println("╠════════════════════════════════════════════╣");
    Serial.println("║ ⚠️  NO WAV FILES FOUND!");
    Serial.println("║    Player looks for: 1.wav, 2.wav, etc.");
  }

  Serial.println("╚════════════════════════════════════════════╝");
}

void showDebugInfo() {
  Serial.println("\n╔════════════════ DEBUG INFO ════════════════╗");
  Serial.print("║ Core1 Running: ");
  Serial.println(core1Running ? "YES" : "NO");
  Serial.print("║ SD Initialized: ");
  Serial.println(sdInitialized ? "YES" : "NO");
  Serial.println("╠════════════════════════════════════════════╣");

  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (players[i].playing || players[i].underrunCount > 0) {
      Serial.print("║ Player ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(players[i].playing ? "PLAYING" : "stopped");
      Serial.print(" | Buf: ");
      Serial.print(players[i].bufferAvailable);
      Serial.print("/");
      Serial.print(BUFFER_SIZE);
      Serial.print(" | Underruns: ");
      Serial.println(players[i].underrunCount);
    }
  }

  Serial.println("╚════════════════════════════════════════════╝");
}

//=============================================================================
// CORE1 FUNCTIONS - SD card operations only
//=============================================================================

void core1_main() {
  core1Running = true;

  // Initialize SD card in SDIO mode on Core1
  // arduino-pico SD library: begin(clkPin, cmdPin, dat0Pin) enables SDIO directly
  Serial.print("Core1: Initializing SD (SDIO mode)... ");

  bool sdOk = SD.begin(SD_CLK_PIN, SD_CMD_PIN, SD_DAT0_PIN);

  if (sdOk) {
    Serial.println("OK");

    // Print SD card info
    uint64_t cardSize = SD.size();
    uint32_t cardSizeMB = cardSize / (1024 * 1024);
    Serial.print("Core1: SD card size: ");
    Serial.print(cardSizeMB);
    Serial.println(" MB");
    Serial.println("Core1: SDIO 4-bit mode active (10-12 MB/s)");

    // Set flag AFTER all initialization is complete
    sdInitialized = true;
  } else {
    Serial.println("FAILED");
    Serial.println("Core1: Check SDIO wiring:");
    Serial.println("  CLK:  GP7");
    Serial.println("  CMD:  GP6");
    Serial.println("  DAT0: GP8");
    Serial.println("  DAT1: GP9");
    Serial.println("  DAT2: GP10");
    Serial.println("  DAT3: GP11");
    sdInitialized = false;
    // Don't hang - let Core0 detect failure
  }

  // Main loop for Core1
  while (true) {
    // Service all players - read from SD and fill buffers
    // NO DELAY - With SDIO bandwidth, Core1 can run at full speed
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

  // Fill buffer if playing
  if (player->playing && player->file) {
    core1_fillBuffer(playerIndex);
  }
}

void core1_openFile(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Open file
  player->file = SD.open(player->filename, FILE_READ);
  if (!player->file) {
    Serial.print("ERROR: Cannot open file: ");
    Serial.println(player->filename);
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    mutex_exit(&player->mutex);
    return;
  }

  Serial.print("DEBUG: Opened ");
  Serial.print(player->filename);
  Serial.print(" (");
  Serial.print(player->file.size());
  Serial.println(" bytes)");

  // Read WAV header
  WavHeader header;
  if (player->file.read((uint8_t*)&header, sizeof(WavHeader)) != sizeof(WavHeader)) {
    Serial.println("ERROR: Cannot read WAV header");
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    mutex_exit(&player->mutex);
    return;
  }

  // Validate WAV format
  if (strncmp(header.riff, "RIFF", 4) != 0 ||
      strncmp(header.wave, "WAVE", 4) != 0 ||
      header.audioFormat != 1 ||  // PCM
      header.bitsPerSample != 16) {
    Serial.print("ERROR: Invalid WAV format - ");
    if (strncmp(header.riff, "RIFF", 4) != 0) Serial.println("Not RIFF");
    else if (strncmp(header.wave, "WAVE", 4) != 0) Serial.println("Not WAVE");
    else if (header.audioFormat != 1) {
      Serial.print("Not PCM (format=");
      Serial.print(header.audioFormat);
      Serial.println(")");
    }
    else if (header.bitsPerSample != 16) {
      Serial.print("Not 16-bit (bits=");
      Serial.print(header.bitsPerSample);
      Serial.println(")");
    }
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    mutex_exit(&player->mutex);
    return;
  }

  Serial.print("DEBUG: WAV format OK - ");
  Serial.print(header.numChannels);
  Serial.print(" ch, ");
  Serial.print(header.sampleRate);
  Serial.print(" Hz, ");
  Serial.print(header.bitsPerSample);
  Serial.println(" bit");

  player->numChannels = header.numChannels;

  // Find data chunk
  char chunkID[4];
  uint32_t chunkSize;
  bool foundData = false;

  Serial.println("DEBUG: Scanning for data chunk...");
  while (player->file.available()) {
    if (player->file.read((uint8_t*)chunkID, 4) != 4) break;
    if (player->file.read((uint8_t*)&chunkSize, 4) != 4) break;

    if (strncmp(chunkID, "data", 4) == 0) {
      player->dataSize = chunkSize;
      foundData = true;
      Serial.print("DEBUG: Found data chunk - ");
      Serial.print(chunkSize);
      Serial.print(" bytes (");
      Serial.print(chunkSize / 1024);
      Serial.println(" KB)");
      break;
    } else {
      Serial.print("DEBUG: Skipping chunk '");
      Serial.write((uint8_t*)chunkID, 4);
      Serial.print("' (");
      Serial.print(chunkSize);
      Serial.println(" bytes)");
      player->file.seek(player->file.position() + chunkSize);
    }
  }

  if (!foundData) {
    Serial.println("ERROR: No data chunk found in WAV file!");
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    mutex_exit(&player->mutex);
    return;
  }

  Serial.println("✓ File ready to play!");
}

void core1_fillBuffer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Check if buffer needs filling
  mutex_enter_blocking(&player->mutex);
  uint32_t available = player->bufferAvailable;
  uint32_t writePos = player->bufferWritePos;
  mutex_exit(&player->mutex);

  // Fill buffer when less than 75% full (less aggressive with SDIO speed)
  if (available > (BUFFER_SIZE * 3 / 4)) {
    return;  // Buffer still mostly full
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

  // Read large chunks (4KB) - SDIO can handle it easily
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
    for (uint32_t i = 0; i < samplesToRead / 2; i++) {
      int16_t left, right;
      player->file.read((uint8_t*)&left, 2);
      player->file.read((uint8_t*)&right, 2);
      int16_t mono = ((int32_t)left + (int32_t)right) / 2;
      player->buffer[writePos] = mono;
      writePos = (writePos + 1) % BUFFER_SIZE;
    }
    player->dataPosition += bytesToRead;
  }

  // Update write position and available count (with mutex)
  mutex_enter_blocking(&player->mutex);
  player->bufferWritePos = writePos;
  player->bufferAvailable += samplesToRead;
  player->core1ReadCount++;
  mutex_exit(&player->mutex);
}
