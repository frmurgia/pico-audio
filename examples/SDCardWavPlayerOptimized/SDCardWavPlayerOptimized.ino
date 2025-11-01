// SD Card WAV Player - OPTIMIZED for Pimoroni Pico Plus
// VERSION: 1.0-OPTIMIZED (16KB buffers with pre-fill)
// DATE: 2025-11-01
//
// Plays up to 10 WAV files simultaneously with pre-buffering
// Optimized for RP2350B with 8MB PSRAM
//
// SD Card Wiring (SPI):
// - SCK (Clock)  -> GP6
// - MOSI (Data In) -> GP7
// - MISO (Data Out) -> GP4
// - CS (Chip Select) -> GP5
//
// Features:
// - Large buffer per player (32KB each) to prevent underruns
// - Intelligent buffer refill strategy
// - Priority-based servicing
// - Optimized for smooth multi-file playback
//
// Controls via Serial:
// - '1'-'9','0' : Play track 1-10
// - 's' : Stop all tracks
// - 'l' : List available WAV files
// - 'v' : Volume control (will prompt for level)

#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <SD.h>
#include <pico-audio.h>

// SD Card pin configuration
#define SD_CS_PIN   5
#define SD_SCK_PIN  6
#define SD_MOSI_PIN 7
#define SD_MISO_PIN 4

// Number of simultaneous players
#define NUM_PLAYERS 10

// Buffer size per player (larger buffer = smoother playback)
// 32KB per player = ~750ms of audio at 44.1kHz
#define BUFFER_SIZE 16384  // 16K samples = 32KB

// WAV file header structure
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

// Player state with large pre-buffer
struct WavPlayer {
  File file;
  AudioPlayQueue queue;
  bool playing;
  uint32_t dataSize;
  uint32_t dataPosition;
  uint16_t numChannels;
  char filename[32];

  // Large buffer for pre-loading
  int16_t* buffer;
  uint32_t bufferSize;
  uint32_t bufferReadPos;
  uint32_t bufferWritePos;
  uint32_t bufferAvailable;

  bool bufferUnderrun;
  uint32_t underrunCount;
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

// Global volume
float globalVolume = 0.1;

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println("\n=== SD WAV Player OPTIMIZED ===");
  Serial.println("VERSION 1.0-OPTIMIZED (2025-11-01)");
  Serial.println("Pimoroni Pico Plus RP2350B");
  Serial.println("Multi-file playback with pre-buffering");

  // Configure SPI for SD
  SPI.setRX(SD_MISO_PIN);
  SPI.setTX(SD_MOSI_PIN);
  SPI.setSCK(SD_SCK_PIN);

  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(" FAILED!");
    while (1) delay(1000);
  }
  Serial.println(" OK");

  // Allocate audio memory
  AudioMemory(120);

  // Configure mixers
  updateMixerGains();

  // Initialize players and allocate buffers
  Serial.println("Allocating player buffers...");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    players[i].playing = false;
    players[i].dataSize = 0;
    players[i].dataPosition = 0;
    players[i].numChannels = 1;
    players[i].bufferSize = BUFFER_SIZE;
    players[i].bufferReadPos = 0;
    players[i].bufferWritePos = 0;
    players[i].bufferAvailable = 0;
    players[i].bufferUnderrun = false;
    players[i].underrunCount = 0;
    snprintf(players[i].filename, 32, "track%d.wav", i + 1);

    // Allocate buffer
    players[i].buffer = (int16_t*)malloc(BUFFER_SIZE * sizeof(int16_t));
    if (players[i].buffer == NULL) {
      Serial.print("ERROR: Failed to allocate buffer for player ");
      Serial.println(i + 1);
    } else {
      Serial.print("  Player ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print((BUFFER_SIZE * 2) / 1024);
      Serial.println(" KB");
    }
  }

  Serial.print("Total buffer memory: ");
  Serial.print((NUM_PLAYERS * BUFFER_SIZE * 2) / 1024);
  Serial.println(" KB");

  // Start I2S
  i2s1.begin();

  Serial.println("\nReady!");
  Serial.println("\nCommands:");
  Serial.println("  1-9, 0 : Play track 1-10");
  Serial.println("  s      : Stop all");
  Serial.println("  l      : List WAV files");
  Serial.println("  v      : Set volume");
  Serial.println();

  listWavFiles();
}

void loop() {
  // Handle serial commands
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd >= '1' && cmd <= '9') {
      int track = cmd - '1';
      playTrack(track);
    } else if (cmd == '0') {
      playTrack(9);
    } else if (cmd == 's' || cmd == 'S') {
      stopAll();
    } else if (cmd == 'l' || cmd == 'L') {
      listWavFiles();
    } else if (cmd == 'v' || cmd == 'V') {
      setVolume();
    }
  }

  // Service all active players
  // Priority: fill buffers with lowest available data first
  for (int priority = 0; priority < 2; priority++) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
      if (players[i].playing) {
        // Priority 0: refill buffers that are low
        // Priority 1: service audio queues
        if (priority == 0) {
          refillBuffer(i);
        } else {
          servicePlayer(i);
        }
      }
    }
  }

  // Print stats every 2 seconds
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 2000) {
    lastStats = millis();

    int activePlayers = 0;
    int totalUnderruns = 0;
    uint32_t minBuffer = BUFFER_SIZE;

    for (int i = 0; i < NUM_PLAYERS; i++) {
      if (players[i].playing) {
        activePlayers++;
        totalUnderruns += players[i].underrunCount;
        if (players[i].bufferAvailable < minBuffer) {
          minBuffer = players[i].bufferAvailable;
        }
      }
    }

    if (activePlayers > 0) {
      Serial.print("Players: ");
      Serial.print(activePlayers);
      Serial.print(" | CPU: ");
      Serial.print(AudioProcessorUsageMax());
      Serial.print("% | Mem: ");
      Serial.print(AudioMemoryUsageMax());
      Serial.print(" | MinBuf: ");
      Serial.print((minBuffer * 100) / BUFFER_SIZE);
      Serial.print("%");

      if (totalUnderruns > 0) {
        Serial.print(" | UNDERRUNS: ");
        Serial.print(totalUnderruns);
      }

      Serial.println();
      AudioProcessorUsageMaxReset();
    }
  }
}

void refillBuffer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Check if buffer needs refilling (less than 25% full)
  if (player->bufferAvailable > (BUFFER_SIZE / 4)) {
    return;  // Buffer still has plenty of data
  }

  // Calculate how much we can read
  uint32_t spaceAvailable = BUFFER_SIZE - player->bufferAvailable;
  uint32_t bytesRemaining = player->dataSize - player->dataPosition;

  if (bytesRemaining == 0) {
    return;  // End of file
  }

  // Read in chunks to avoid blocking too long
  uint32_t chunkSize = min(spaceAvailable, (uint32_t)2048);  // 2K samples at a time
  uint32_t bytesToRead = min(chunkSize * 2, bytesRemaining);
  uint32_t samplesToRead = bytesToRead / 2;

  if (player->numChannels == 1) {
    // Mono - read directly into circular buffer
    for (uint32_t i = 0; i < samplesToRead; i++) {
      int16_t sample;
      player->file.read((uint8_t*)&sample, 2);
      player->buffer[player->bufferWritePos] = sample;
      player->bufferWritePos = (player->bufferWritePos + 1) % BUFFER_SIZE;
    }
    player->bufferAvailable += samplesToRead;
    player->dataPosition += bytesToRead;

  } else {
    // Stereo - mix to mono while reading
    for (uint32_t i = 0; i < samplesToRead; i++) {
      int16_t left, right;
      player->file.read((uint8_t*)&left, 2);
      player->file.read((uint8_t*)&right, 2);

      int32_t mixed = ((int32_t)left + (int32_t)right) / 2;
      player->buffer[player->bufferWritePos] = (int16_t)mixed;
      player->bufferWritePos = (player->bufferWritePos + 1) % BUFFER_SIZE;
    }
    player->bufferAvailable += samplesToRead;
    player->dataPosition += (bytesToRead * 2);
  }
}

void servicePlayer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Check if queue needs more data
  int16_t* queueBuffer = player->queue.getBuffer();
  if (queueBuffer == NULL) {
    return;  // Queue is full
  }

  // Check if we have data in our buffer
  if (player->bufferAvailable < 128) {
    // Buffer underrun!
    if (!player->bufferUnderrun) {
      player->bufferUnderrun = true;
      player->underrunCount++;
    }

    // Send silence
    memset(queueBuffer, 0, 128 * sizeof(int16_t));
    player->queue.playBuffer();

    // Check if end of file
    if (player->dataPosition >= player->dataSize) {
      stopPlayer(playerIndex);
    }
    return;
  }

  player->bufferUnderrun = false;

  // Copy from circular buffer to queue
  for (int i = 0; i < 128; i++) {
    queueBuffer[i] = player->buffer[player->bufferReadPos];
    player->bufferReadPos = (player->bufferReadPos + 1) % BUFFER_SIZE;
  }

  player->bufferAvailable -= 128;
  player->queue.playBuffer();
}

void playTrack(int playerIndex) {
  if (playerIndex < 0 || playerIndex >= NUM_PLAYERS) return;

  WavPlayer* player = &players[playerIndex];

  // Stop if already playing
  if (player->playing) {
    stopPlayer(playerIndex);
  }

  Serial.print("Loading ");
  Serial.print(player->filename);
  Serial.print("...");

  // Open file
  player->file = SD.open(player->filename);
  if (!player->file) {
    Serial.println(" FILE NOT FOUND");
    return;
  }

  // Read WAV header
  WavHeader header;
  player->file.read((uint8_t*)&header, sizeof(WavHeader));

  if (strncmp(header.riff, "RIFF", 4) != 0 ||
      strncmp(header.wave, "WAVE", 4) != 0) {
    Serial.println(" INVALID WAV");
    player->file.close();
    return;
  }

  if (header.audioFormat != 1 || header.bitsPerSample != 16) {
    Serial.println(" UNSUPPORTED FORMAT");
    player->file.close();
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
    Serial.println(" NO DATA CHUNK");
    player->file.close();
    return;
  }

  // Initialize player state
  player->dataSize = chunkSize;
  player->dataPosition = 0;
  player->numChannels = header.numChannels;
  player->bufferReadPos = 0;
  player->bufferWritePos = 0;
  player->bufferAvailable = 0;
  player->bufferUnderrun = false;
  player->underrunCount = 0;
  player->playing = true;

  // Pre-fill buffer before starting playback
  Serial.print(" Pre-buffering");
  for (int i = 0; i < 8; i++) {  // Fill buffer 8 times
    refillBuffer(playerIndex);
    Serial.print(".");
  }

  Serial.print(" OK (");
  Serial.print(player->dataSize / 1024);
  Serial.print(" KB, ");
  Serial.print(header.numChannels == 1 ? "mono" : "stereo");
  Serial.print(", buf: ");
  Serial.print((player->bufferAvailable * 100) / BUFFER_SIZE);
  Serial.println("%)");
}

void stopPlayer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  if (player->playing) {
    player->file.close();
    player->playing = false;
    player->bufferAvailable = 0;

    Serial.print("Stopped player ");
    Serial.print(playerIndex + 1);
    if (player->underrunCount > 0) {
      Serial.print(" (");
      Serial.print(player->underrunCount);
      Serial.print(" underruns)");
    }
    Serial.println();
  }
}

void stopAll() {
  Serial.println("Stopping all players...");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    stopPlayer(i);
  }
}

void updateMixerGains() {
  float gain = globalVolume / 10.0;  // Divide by number of possible players
  for (int i = 0; i < 4; i++) {
    mixer1.gain(i, gain);
    mixer2.gain(i, gain);
    mixer3.gain(i, gain);
  }
}

void setVolume() {
  Serial.println("\nEnter volume (0-100): ");

  // Wait for input
  while (!Serial.available()) {
    delay(10);
  }

  int vol = Serial.parseInt();
  if (vol < 0) vol = 0;
  if (vol > 100) vol = 100;

  globalVolume = vol / 100.0;
  updateMixerGains();

  Serial.print("Volume set to ");
  Serial.print(vol);
  Serial.println("%");
}

void listWavFiles() {
  Serial.println("\n=== Available WAV Files ===");

  File root = SD.open("/");
  int wavCount = 0;

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      const char* name = entry.name();
      int len = strlen(name);

      if (len > 4 && strcasecmp(name + len - 4, ".wav") == 0) {
        wavCount++;
        Serial.print("  ");
        Serial.print(name);
        Serial.print(" (");
        Serial.print(entry.size() / 1024);
        Serial.println(" KB)");
      }
    }
    entry.close();
  }

  root.close();

  if (wavCount == 0) {
    Serial.println("  No WAV files found!");
  } else {
    Serial.print("\nFound ");
    Serial.print(wavCount);
    Serial.println(" WAV file(s)");
  }
  Serial.println();
}
