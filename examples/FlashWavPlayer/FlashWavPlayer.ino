// Flash WAV Player - LittleFS VERSION
// VERSION: 1.0
// DATE: 2025-11-02
//
// Reads WAV files from FLASH MEMORY instead of SD card!
// Uses LittleFS filesystem stored in RP2350 flash (14MB available)
//
// ADVANTAGES over SD card:
// - Faster: ~3-4 MB/s (vs ~1-2 MB/s SPI SD)
// - No SD card needed
// - No wiring needed
// - More reliable (no card insertion issues)
//
// LIMITATIONS:
// - Only ~14MB available (10-15 WAV files depending on size)
// - Flash wear (but rated for 100,000+ writes)
//
// HOW TO UPLOAD WAV FILES TO FLASH:
// 1. Use Pico-W-Go or similar tool
// 2. Or use LittleFS upload via USB (see README)
// 3. Files go in /littlefs/ directory
//
// Dual-Core Architecture:
// - Core0: Audio processing (AudioPlayQueue, mixers, I2S)
// - Core1: Flash reading (fills buffers from LittleFS)
// - Communication: Thread-safe circular buffers
//
// I2S Pin Configuration (PCM5102 DAC):
// - BCK (Bit Clock)    -> GP20
// - LRCK (Word Select) -> GP21
// - DIN (Data)         -> GP22
//
// Performance:
// - 10 files @ 44.1kHz stereo = 1.76 MB/s required
// - LittleFS provides ~3-4 MB/s = 2x headroom
// - 16KB buffers per player = smooth playback
//
// Controls via Serial:
// - '1'-'9','0' : Play/stop track 1-10
// - 's' : Stop all
// - 'l' : List files in flash
// - 'd' : Debug info
// - 'v'/'b' : Volume up/down

#include <Adafruit_TinyUSB.h>
#include <LittleFS.h>
#include <pico-audio.h>
#include <pico/multicore.h>
#include <pico/mutex.h>

// LittleFS configuration
#define FLASH_FILESYSTEM_SIZE (14 * 1024 * 1024)  // 14MB for files

// Performance tuning
#define NUM_PLAYERS 10
#define BUFFER_SIZE (16 * 1024)  // 16KB per player = 372ms @ 44.1kHz stereo
#define READ_CHUNK_SIZE (4 * 1024)  // 4KB per flash read

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

// Player state
struct WavPlayer {
  File file;  // LittleFS file handle (Core1 only)
  AudioPlayQueue queue;  // Audio queue (Core0 only)

  // Shared state
  volatile bool playing;
  volatile bool stopRequested;
  uint32_t dataSize;
  uint32_t dataPosition;
  uint16_t numChannels;
  char filename[32];

  // Circular buffer
  int16_t* buffer;
  volatile uint32_t bufferSize;
  volatile uint32_t bufferReadPos;
  volatile uint32_t bufferWritePos;
  volatile uint32_t bufferAvailable;

  // Stats
  volatile uint32_t underrunCount;
  volatile uint32_t totalBytesRead;

  mutex_t mutex;
};

// Audio components
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
float globalVolume = 0.3;
volatile bool core1Running = false;
volatile bool flashInitialized = false;

// Forward declarations
void core1_main();
void playTrack(int playerIndex);
void stopPlayer(int playerIndex);
void stopAll();
void serviceAudioQueue(int playerIndex);
void core1_servicePlayer(int playerIndex);
void updateMixerGains();
void showDebugInfo();
void listFlashFiles();

//=============================================================================
// CORE0 FUNCTIONS
//=============================================================================

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  Flash WAV Player - LittleFS          ║");
  Serial.println("║  VERSION 1.0 (2025-11-02)             ║");
  Serial.println("║  RP2350B - No SD Card Required!       ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  // Initialize players
  Serial.print("Initializing players... ");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    mutex_init(&players[i].mutex);
    players[i].buffer = (int16_t*)malloc(BUFFER_SIZE * sizeof(int16_t));
    if (!players[i].buffer) {
      Serial.print("FAILED - Out of memory at player ");
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
    snprintf(players[i].filename, sizeof(players[i].filename), "/track%d.wav", i + 1);
  }
  Serial.println("OK");

  // Initialize audio
  Serial.print("Initializing audio (44.1kHz I2S)... ");
  AudioMemory(20);
  i2s1.begin();
  updateMixerGains();
  Serial.println("OK");

  // Launch Core1
  Serial.print("Launching Core1... ");
  multicore_launch_core1(core1_main);

  // Wait for Core1 to initialize flash
  unsigned long timeout = millis();
  while (!flashInitialized && (millis() - timeout < 10000)) {
    delay(50);
  }

  if (core1Running && flashInitialized) {
    Serial.println("OK");
    Serial.println("Flash: ✓ READY");
  } else {
    Serial.println("FAILED");
    Serial.println("Flash: ✗ LittleFS initialization failed");
  }

  Serial.println();
  Serial.println("Configuration:");
  Serial.print("  Flash filesystem: ");
  Serial.print(FLASH_FILESYSTEM_SIZE / (1024 * 1024));
  Serial.println(" MB");
  Serial.print("  Buffer per player: ");
  Serial.print(BUFFER_SIZE / 1024);
  Serial.println(" KB");
  Serial.print("  Read speed: ~3-4 MB/s");
  Serial.println();

  Serial.println();
  Serial.println("Ready! Commands:");
  Serial.println("  '1'-'0' = Play track | 's' = Stop all");
  Serial.println("  'l' = List files | 'd' = Debug");
  Serial.println("  'v' = Vol+ | 'b' = Vol-");
  Serial.println();
}

void loop() {
  // Handle serial commands
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd >= '1' && cmd <= '9') {
      playTrack(cmd - '1');
    } else if (cmd == '0') {
      playTrack(9);
    } else if (cmd == 's' || cmd == 'S') {
      stopAll();
    } else if (cmd == 'l' || cmd == 'L') {
      listFlashFiles();
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

  // Service audio queues
  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (players[i].playing) {
      serviceAudioQueue(i);
    }
  }

  // Stats every 2 seconds
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

  delay(1);
}

void serviceAudioQueue(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  int16_t* queueBuffer = player->queue.getBuffer();
  if (queueBuffer == NULL) {
    return;
  }

  mutex_enter_blocking(&player->mutex);
  uint32_t available = player->bufferAvailable;

  if (available < 128) {
    player->underrunCount++;
    mutex_exit(&player->mutex);
    memset(queueBuffer, 0, 128 * sizeof(int16_t));
    player->queue.playBuffer();
    return;
  }

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
    stopPlayer(playerIndex);
    Serial.print("Stopped track ");
    Serial.println(playerIndex + 1);
  } else {
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
  for (int i = 0; i < 4; i++) {
    mixer1.gain(i, globalVolume);
    mixer2.gain(i, globalVolume);
    mixer3.gain(i, globalVolume);
  }
}

void showDebugInfo() {
  Serial.println("\n===== DEBUG INFO =====");
  Serial.print("CPU: ");
  Serial.print(AudioProcessorUsageMax());
  Serial.print("% | Mem: ");
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
      Serial.print("% | Underruns: ");
      Serial.println(players[i].underrunCount);
    }
  }
  Serial.println("====================\n");
}

void listFlashFiles() {
  Serial.println("\n===== FILES IN FLASH =====");

  Dir root = LittleFS.openDir("/");
  uint32_t totalSize = 0;
  int fileCount = 0;

  while (root.next()) {
    String fileName = root.fileName();
    File f = root.openFile("r");
    uint32_t fileSize = f.size();
    f.close();

    Serial.print("  ");
    Serial.print(fileName);
    Serial.print(" (");
    Serial.print(fileSize / 1024);
    Serial.println(" KB)");

    totalSize += fileSize;
    fileCount++;
  }

  Serial.println();
  Serial.print("Total: ");
  Serial.print(fileCount);
  Serial.print(" files, ");
  Serial.print(totalSize / 1024);
  Serial.print(" KB / ");
  Serial.print(FLASH_FILESYSTEM_SIZE / 1024);
  Serial.println(" KB available");

  FSInfo fs_info;
  LittleFS.info(fs_info);
  Serial.print("Used: ");
  Serial.print((fs_info.usedBytes * 100) / fs_info.totalBytes);
  Serial.println("%");
  Serial.println("==========================\n");
}

//=============================================================================
// CORE1 FUNCTIONS - Flash reading
//=============================================================================

void core1_main() {
  core1Running = true;

  Serial.print("Core1: Initializing LittleFS... ");

  // Initialize LittleFS
  if (LittleFS.begin()) {
    Serial.println("OK");

    FSInfo fs_info;
    LittleFS.info(fs_info);
    Serial.print("Core1: Flash capacity: ");
    Serial.print(fs_info.totalBytes / (1024 * 1024));
    Serial.print(" MB, Used: ");
    Serial.print(fs_info.usedBytes / (1024 * 1024));
    Serial.println(" MB");

    flashInitialized = true;
  } else {
    Serial.println("FAILED");
    Serial.println("Core1: LittleFS mount failed!");
    Serial.println("       You may need to format flash first");
    flashInitialized = false;
  }

  // Main loop - runs at maximum speed
  while (true) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
      core1_servicePlayer(i);
    }
  }
}

void core1_servicePlayer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Open file if needed
  if (player->playing && !player->file && !player->stopRequested) {
    player->file = LittleFS.open(player->filename, "r");

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

      player->file.seek(player->file.position() + chunkSize);
    }
  }

  // Close file if stopping
  if (player->stopRequested && player->file) {
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    player->stopRequested = false;
    mutex_exit(&player->mutex);
    return;
  }

  // Fill buffer
  if (!player->playing || !player->file) {
    return;
  }

  mutex_enter_blocking(&player->mutex);
  uint32_t available = player->bufferAvailable;
  uint32_t freeSpace = player->bufferSize - available;
  mutex_exit(&player->mutex);

  if (freeSpace < (BUFFER_SIZE / 2)) {
    return;  // Buffer still more than half full
  }

  // Read chunk from flash
  uint32_t toRead = min(freeSpace * 2, (uint32_t)READ_CHUNK_SIZE);
  toRead = min(toRead, player->dataSize - player->dataPosition);

  if (toRead == 0) {
    // Loop file
    player->file.seek(player->file.position() - player->dataSize);
    player->dataPosition = 0;
    return;
  }

  static int16_t tempBuffer[READ_CHUNK_SIZE / 2];
  int bytesRead = player->file.read((uint8_t*)tempBuffer, toRead);

  if (bytesRead > 0) {
    player->dataPosition += bytesRead;
    player->totalBytesRead += bytesRead;

    int samplesRead = bytesRead / 2;

    mutex_enter_blocking(&player->mutex);

    for (int i = 0; i < samplesRead; i++) {
      int16_t sample = tempBuffer[i];

      // Convert stereo to mono if needed
      if (player->numChannels == 2 && i % 2 == 1) {
        continue;
      }

      player->buffer[player->bufferWritePos] = sample;
      player->bufferWritePos = (player->bufferWritePos + 1) % player->bufferSize;
      player->bufferAvailable++;

      if (player->bufferAvailable >= player->bufferSize) {
        break;
      }
    }

    mutex_exit(&player->mutex);
  }
}
