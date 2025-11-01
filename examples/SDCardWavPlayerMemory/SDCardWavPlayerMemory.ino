// SD Card WAV Player - MEMORY CACHED VERSION
// VERSION: 2.0 (pre-loads all WAV files to PSRAM)
// DATE: 2025-11-01
//
// Strategy: Load all WAV files to memory at startup, then play from RAM
// This eliminates ALL SD card access during playback = ZERO latency!
//
// Pimoroni Pico Plus has 8MB PSRAM - plenty for audio files
// Example: 10 x 500KB files = 5MB (fits easily)
//
// SD Card Wiring (SPI):
// - SCK (Clock)  -> GP6
// - MOSI (Data In) -> GP7
// - MISO (Data Out) -> GP4
// - CS (Chip Select) -> GP5
//
// Controls via Serial:
// - '1'-'9','0' : Play track 1-10
// - 's' : Stop all tracks
// - 'i' : Show memory info

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

// Memory-cached WAV file
struct CachedWavFile {
  char filename[32];
  int16_t* audioData;      // Audio samples in memory
  uint32_t numSamples;     // Total samples
  uint16_t numChannels;    // 1=mono, 2=stereo
  bool loaded;
};

// Player state
struct MemoryPlayer {
  AudioPlayQueue queue;
  CachedWavFile* wavFile;  // Pointer to cached file
  volatile bool playing;
  uint32_t playPosition;   // Current sample position
};

// Audio components
MemoryPlayer players[NUM_PLAYERS];
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
CachedWavFile cachedFiles[NUM_PLAYERS];
float globalVolume = 0.3;

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  SD WAV Player - MEMORY CACHED        ║");
  Serial.println("║  VERSION 2.0 (2025-11-01)             ║");
  Serial.println("║  Pre-loads files to PSRAM @ startup   ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();

  // Configure SPI for SD
  SPI.setRX(SD_MISO_PIN);
  SPI.setTX(SD_MOSI_PIN);
  SPI.setSCK(SD_SCK_PIN);

  Serial.print("Initializing SD card... ");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("FAILED!");
    Serial.println("Check SD card and wiring");
    while (1) delay(1000);
  }
  Serial.println("OK");

  // Initialize audio
  AudioMemory(120);

  // Configure mixers
  updateMixerGains();

  // Initialize I2S
  i2s1.begin();

  // Initialize players
  for (int i = 0; i < NUM_PLAYERS; i++) {
    players[i].playing = false;
    players[i].playPosition = 0;
    players[i].wavFile = NULL;
    players[i].queue.begin();
  }

  // Pre-load all WAV files from SD to memory
  Serial.println();
  Serial.println("═══════════════════════════════════════");
  Serial.println("Loading WAV files to memory...");
  Serial.println("═══════════════════════════════════════");

  loadAllWavFiles();

  Serial.println("═══════════════════════════════════════");
  Serial.println();
  showMemoryInfo();

  Serial.println("\nReady!");
  Serial.println("Commands: '1'-'0' = play track, 's' = stop all, 'i' = info");
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
    } else if (cmd == 'i' || cmd == 'I') {
      showMemoryInfo();
    }
  }

  // Service audio queues - feed from memory
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
    for (int i = 0; i < NUM_PLAYERS; i++) {
      if (players[i].playing) {
        activePlayers++;
      }
    }

    if (activePlayers > 0) {
      Serial.print("♪ Players: ");
      Serial.print(activePlayers);
      Serial.print(" | CPU: ");
      Serial.print(AudioProcessorUsageMax());
      Serial.print("% | Mem: ");
      Serial.println(AudioMemoryUsageMax());
      AudioProcessorUsageMaxReset();
    }
  }

  // Small delay for stability
  delay(1);
}

void serviceAudioQueue(int playerIndex) {
  MemoryPlayer* player = &players[playerIndex];

  // Check if queue needs data
  int16_t* queueBuffer = player->queue.getBuffer();
  if (queueBuffer == NULL) {
    return;  // Queue is full
  }

  CachedWavFile* wav = player->wavFile;
  if (wav == NULL || !wav->loaded) {
    memset(queueBuffer, 0, 128 * sizeof(int16_t));
    player->queue.playBuffer();
    player->playing = false;
    return;
  }

  // Check if we've reached the end
  if (player->playPosition >= wav->numSamples) {
    player->playing = false;
    memset(queueBuffer, 0, 128 * sizeof(int16_t));
    player->queue.playBuffer();
    return;
  }

  // Copy 128 samples from memory
  uint32_t samplesRemaining = wav->numSamples - player->playPosition;
  uint32_t samplesToCopy = min((uint32_t)128, samplesRemaining);

  if (wav->numChannels == 1) {
    // Mono - copy directly
    for (uint32_t i = 0; i < samplesToCopy; i++) {
      queueBuffer[i] = wav->audioData[player->playPosition + i];
    }
  } else {
    // Stereo - mix to mono
    for (uint32_t i = 0; i < samplesToCopy; i++) {
      uint32_t stereoPos = (player->playPosition + i) * 2;
      int32_t left = wav->audioData[stereoPos];
      int32_t right = wav->audioData[stereoPos + 1];
      queueBuffer[i] = (left + right) / 2;
    }
  }

  // Fill remaining with silence if needed
  for (uint32_t i = samplesToCopy; i < 128; i++) {
    queueBuffer[i] = 0;
  }

  player->playPosition += samplesToCopy;
  player->queue.playBuffer();
}

void loadAllWavFiles() {
  File root = SD.open("/");
  if (!root) {
    Serial.println("ERROR: Cannot open root directory");
    return;
  }

  int fileCount = 0;

  while (fileCount < NUM_PLAYERS) {
    File entry = root.openNextFile();
    if (!entry) break;  // No more files

    if (!entry.isDirectory()) {
      String filename = String(entry.name());
      filename.toLowerCase();

      if (filename.endsWith(".wav")) {
        Serial.print("Loading: ");
        Serial.print(entry.name());
        Serial.print(" (");
        Serial.print(entry.size());
        Serial.print(" bytes)... ");

        if (loadWavToMemory(entry, &cachedFiles[fileCount])) {
          strncpy(cachedFiles[fileCount].filename, entry.name(), 31);
          cachedFiles[fileCount].filename[31] = '\0';
          Serial.println("OK");
          fileCount++;
        } else {
          Serial.println("FAILED");
        }
      }
    }
    entry.close();
  }

  root.close();

  Serial.println();
  Serial.print("Loaded ");
  Serial.print(fileCount);
  Serial.println(" files to memory");
}

bool loadWavToMemory(File &file, CachedWavFile* cached) {
  cached->loaded = false;
  cached->audioData = NULL;
  cached->numSamples = 0;

  // Read WAV header
  WavHeader header;
  file.seek(0);

  if (file.read((uint8_t*)&header, sizeof(WavHeader)) != sizeof(WavHeader)) {
    return false;
  }

  // Validate WAV format
  if (strncmp(header.riff, "RIFF", 4) != 0 ||
      strncmp(header.wave, "WAVE", 4) != 0) {
    return false;
  }

  if (header.audioFormat != 1) {  // PCM
    return false;
  }

  if (header.bitsPerSample != 16) {
    return false;
  }

  cached->numChannels = header.numChannels;

  // Find data chunk
  char chunkID[4];
  uint32_t chunkSize;

  while (file.available()) {
    if (file.read((uint8_t*)chunkID, 4) != 4) break;
    if (file.read((uint8_t*)&chunkSize, 4) != 4) break;

    if (strncmp(chunkID, "data", 4) == 0) {
      // Found data chunk - allocate memory
      uint32_t numSamples = chunkSize / 2;  // 16-bit samples

      cached->audioData = (int16_t*)malloc(chunkSize);
      if (cached->audioData == NULL) {
        Serial.print("MALLOC FAILED for ");
        Serial.print(chunkSize);
        Serial.print(" bytes");
        return false;
      }

      // Read all audio data to memory
      uint32_t bytesRead = file.read((uint8_t*)cached->audioData, chunkSize);

      if (bytesRead == chunkSize) {
        cached->numSamples = numSamples;
        cached->loaded = true;
        return true;
      } else {
        free(cached->audioData);
        cached->audioData = NULL;
        return false;
      }
    } else {
      // Skip this chunk
      file.seek(file.position() + chunkSize);
    }
  }

  return false;
}

void playTrack(int trackIndex) {
  if (trackIndex < 0 || trackIndex >= NUM_PLAYERS) return;

  if (!cachedFiles[trackIndex].loaded) {
    Serial.print("Track ");
    Serial.print(trackIndex + 1);
    Serial.println(" not loaded");
    return;
  }

  // Find available player or reuse same index
  int playerIndex = trackIndex;

  if (players[playerIndex].playing) {
    // Stop current playback
    players[playerIndex].playing = false;
    delay(10);
  }

  // Start playback
  players[playerIndex].wavFile = &cachedFiles[trackIndex];
  players[playerIndex].playPosition = 0;
  players[playerIndex].playing = true;

  Serial.print("▶ Playing track ");
  Serial.print(trackIndex + 1);
  Serial.print(": ");
  Serial.println(cachedFiles[trackIndex].filename);
}

void stopAll() {
  Serial.println("■ Stopping all tracks");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    players[i].playing = false;
    players[i].playPosition = 0;
  }
}

void updateMixerGains() {
  for (int i = 0; i < 4; i++) {
    mixer1.gain(i, globalVolume);
    mixer2.gain(i, globalVolume);
    mixer3.gain(i, globalVolume);
  }
}

void showMemoryInfo() {
  Serial.println("\n╔════════════════ MEMORY INFO ═══════════════╗");

  uint32_t totalBytes = 0;
  uint32_t totalSamples = 0;

  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (cachedFiles[i].loaded) {
      Serial.print("║ Track ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(cachedFiles[i].filename);

      uint32_t bytes = cachedFiles[i].numSamples * 2;
      totalBytes += bytes;
      totalSamples += cachedFiles[i].numSamples;

      Serial.print(" (");
      Serial.print(bytes / 1024);
      Serial.print(" KB, ");

      float seconds = (float)cachedFiles[i].numSamples / 44100.0;
      if (cachedFiles[i].numChannels == 2) {
        seconds /= 2.0;
      }
      Serial.print(seconds, 1);
      Serial.println(" sec)");
    }
  }

  Serial.println("╠════════════════════════════════════════════╣");
  Serial.print("║ Total memory used: ");
  Serial.print(totalBytes / 1024);
  Serial.print(" KB (");
  Serial.print(totalBytes / 1024.0 / 1024.0, 2);
  Serial.println(" MB)");

  Serial.print("║ PSRAM available: 8 MB");
  Serial.print(" | Used: ");
  Serial.print((totalBytes / 1024.0 / 1024.0 / 8.0) * 100.0, 1);
  Serial.println("%");

  Serial.println("╚════════════════════════════════════════════╝");
}
