// SD Card WAV Player - MEMORY CACHED VERSION
// VERSION: 2.4 (Practical memory limits)
// DATE: 2025-11-02
//
// Strategy: Load WAV files to memory at startup, then play from RAM
// This eliminates ALL SD card access during playback = ZERO latency!
//
// IMPORTANT: Due to memory constraints, only files < 500KB each will be loaded
// For larger files, please use the streaming player (SDCardWavPlayerSDIO)
// Total memory budget: ~2MB (normal RAM + available heap)
//
// SD Card Wiring (SDIO 4-bit mode):
// - CLK  -> GP7
// - CMD  -> GP6
// - DAT0 -> GP8 (DAT1=GP9, DAT2=GP10, DAT3=GP11)
//
// Controls via Serial:
// - '1'-'9','0' : Play track 1-10
// - 's' : Stop all tracks
// - 'i' : Show memory info

#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <SD.h>
#include <pico-audio.h>

// SD Card pin configuration (SDIO 4-bit)
#define SD_CLK_PIN  7
#define SD_CMD_PIN  6
#define SD_DAT0_PIN 8  // DAT1=9, DAT2=10, DAT3=11 (must be consecutive!)

// Number of simultaneous players
#define NUM_PLAYERS 10

// Memory limits - practical constraints for stable operation
#define MAX_FILE_SIZE_KB 500        // Max file size to load (500KB per file)
#define MAX_TOTAL_MEMORY_KB 2000    // Max total memory usage (2MB)

// Memory tracking
uint32_t totalMemoryUsed = 0;

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
  Serial.println("║  VERSION 2.4 (2025-11-02)             ║");
  Serial.println("║  Practical memory limits              ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();
  Serial.print("⚠️  File size limit: ");
  Serial.print(MAX_FILE_SIZE_KB);
  Serial.println(" KB per file");
  Serial.print("⚠️  Total memory limit: ");
  Serial.print(MAX_TOTAL_MEMORY_KB);
  Serial.println(" KB");
  Serial.println("    (Larger files will be skipped)");
  Serial.println();

  // Initialize SD card in SDIO 4-bit mode
  Serial.print("Initializing SD card (SDIO 4-bit)... ");

  bool sdOk = SD.begin(SD_CLK_PIN, SD_CMD_PIN, SD_DAT0_PIN);

  if (!sdOk) {
    Serial.println("FAILED!");
    Serial.println("Check SD card and SDIO wiring:");
    Serial.println("  CLK:  GP7");
    Serial.println("  CMD:  GP6");
    Serial.println("  DAT0: GP8");
    Serial.println("  DAT1: GP9");
    Serial.println("  DAT2: GP10");
    Serial.println("  DAT3: GP11");
    while (1) delay(1000);
  }

  Serial.println("OK (SDIO 4-bit @ 10-12 MB/s)");

  // Show SD card info
  uint64_t cardSize = SD.size();
  Serial.print("SD card size: ");
  Serial.print(cardSize / (1024 * 1024));
  Serial.println(" MB");

  // List ALL files on SD card
  Serial.println();
  Serial.println("Files on SD card:");
  Serial.println("═════════════════════════════════");
  File root = SD.open("/");
  if (root) {
    int totalFiles = 0;
    int wavFiles = 0;
    while (true) {
      File entry = root.openNextFile();
      if (!entry) break;

      totalFiles++;
      String filename = String(entry.name());
      filename.toLowerCase();

      Serial.print("  ");
      Serial.print(entry.name());
      Serial.print(" (");
      Serial.print(entry.size() / 1024);
      Serial.print(" KB)");

      if (filename.endsWith(".wav")) {
        Serial.print(" ← WAV file");
        wavFiles++;
      }
      Serial.println();

      entry.close();
    }
    root.close();

    Serial.println("═════════════════════════════════");
    Serial.print("Total files: ");
    Serial.print(totalFiles);
    Serial.print(" | WAV files: ");
    Serial.println(wavFiles);

    if (wavFiles == 0) {
      Serial.println();
      Serial.println("⚠️  NO WAV FILES FOUND!");
      Serial.println("   Make sure your SD card has .wav files");
      Serial.println("   Files must be named: track1.wav, track2.wav, etc.");
    }
  } else {
    Serial.println("ERROR: Cannot open root directory!");
  }
  Serial.println();

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
    // Note: AudioPlayQueue doesn't have begin() - constructor handles init
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

  // Auto-play all loaded tracks
  Serial.println();
  Serial.println("Auto-starting all loaded tracks...");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (cachedFiles[i].loaded) {
      playTrack(i);
      delay(50);  // Small delay between starts
    }
  }

  Serial.println("\nReady!");
  Serial.println("Commands: '1'-'0' = toggle track, 's' = stop all, 'i' = info");
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

// Helper function to get free PSRAM
uint32_t getFreePSRAM() {
  #ifdef PICO_PSRAM_SIZE
    // Try to estimate free PSRAM by attempting allocation
    // RP2350B has 8MB PSRAM
    return 8 * 1024 * 1024 - totalMemoryUsed;
  #else
    return 0;
  #endif
}

void loadAllWavFiles() {
  Serial.println("DEBUG: Opening root directory...");
  File root = SD.open("/");
  if (!root) {
    Serial.println("❌ ERROR: Cannot open root directory!");
    Serial.println("   SD card may not be initialized properly");
    return;
  }
  Serial.println("✓ Root directory opened");

  int fileCount = 0;
  int filesScanned = 0;

  Serial.println();
  Serial.println("Scanning for WAV files...");
  Serial.println();

  while (fileCount < NUM_PLAYERS) {
    File entry = root.openNextFile();
    if (!entry) {
      Serial.println("DEBUG: No more files in directory");
      break;  // No more files
    }

    filesScanned++;
    Serial.print("DEBUG: Found file ");
    Serial.print(filesScanned);
    Serial.print(": ");
    Serial.println(entry.name());

    if (entry.isDirectory()) {
      Serial.println("  → Skipped (directory)");
      entry.close();
      continue;
    }

    String filename = String(entry.name());
    filename.toLowerCase();

    if (!filename.endsWith(".wav")) {
      Serial.print("  → Skipped (not .wav, extension: ");
      int dotPos = filename.lastIndexOf('.');
      if (dotPos >= 0) {
        Serial.print(filename.substring(dotPos));
      } else {
        Serial.print("none");
      }
      Serial.println(")");
      entry.close();
      continue;
    }

    Serial.println("  → WAV file detected!");
    uint32_t fileSize = entry.size();
    uint32_t fileSizeKB = fileSize / 1024;
    Serial.print("  → Size: ");
    Serial.print(fileSize);
    Serial.print(" bytes (");
    Serial.print(fileSizeKB);
    Serial.println(" KB)");

    // Check if file is too large
    if (fileSizeKB > MAX_FILE_SIZE_KB) {
      Serial.print("  ⚠️  SKIPPING: File too large (");
      Serial.print(fileSizeKB);
      Serial.print(" KB > ");
      Serial.print(MAX_FILE_SIZE_KB);
      Serial.println(" KB limit)");
      Serial.println("     Use SDCardWavPlayerSDIO for large files");
      entry.close();
      continue;
    }

    // Check if this file would exceed total memory limit
    if (totalMemoryUsed + fileSize > (MAX_TOTAL_MEMORY_KB * 1024)) {
      Serial.print("  ⚠️  SKIPPING: Would exceed total memory limit (");
      Serial.print(totalMemoryUsed / 1024);
      Serial.print(" KB used + ");
      Serial.print(fileSizeKB);
      Serial.print(" KB > ");
      Serial.print(MAX_TOTAL_MEMORY_KB);
      Serial.println(" KB limit)");
      entry.close();
      continue;
    }

    Serial.print("▶ Loading track ");
    Serial.println(fileCount + 1);

    if (loadWavToMemory(entry, &cachedFiles[fileCount])) {
      strncpy(cachedFiles[fileCount].filename, entry.name(), 31);
      cachedFiles[fileCount].filename[31] = '\0';

      Serial.print("✓ Loaded successfully: ");
      Serial.print(cachedFiles[fileCount].numSamples * 2 / 1024);
      Serial.print(" KB (");
      Serial.print(cachedFiles[fileCount].numSamples);
      Serial.print(" samples, ");
      Serial.print(cachedFiles[fileCount].numChannels);
      Serial.println(" channels)");

      fileCount++;
    } else {
      Serial.println("❌ FAILED to load");
    }

    entry.close();
  }

  root.close();

  Serial.println();
  Serial.println("═══════════════════════════════════════");
  Serial.print("✓ Successfully loaded ");
  Serial.print(fileCount);
  Serial.print(" / ");
  Serial.print(filesScanned);
  Serial.println(" files scanned");
  Serial.print("PSRAM used: ");
  Serial.print(psramPoolUsed / 1024);
  Serial.print(" KB / ");
  Serial.print(PSRAM_POOL_SIZE / 1024);
  Serial.print(" KB (");
  Serial.print((psramPoolUsed * 100) / PSRAM_POOL_SIZE);
  Serial.println("%)");
  Serial.println("═══════════════════════════════════════");
}

bool loadWavToMemory(File &file, CachedWavFile* cached) {
  Serial.println("  DEBUG: loadWavToMemory starting...");

  cached->loaded = false;
  cached->audioData = NULL;
  cached->numSamples = 0;

  // Read WAV header
  WavHeader header;
  file.seek(0);
  Serial.print("  DEBUG: Reading WAV header (");
  Serial.print(sizeof(WavHeader));
  Serial.println(" bytes)...");

  int headerBytesRead = file.read((uint8_t*)&header, sizeof(WavHeader));
  Serial.print("  DEBUG: Read ");
  Serial.print(headerBytesRead);
  Serial.println(" bytes");

  if (headerBytesRead != sizeof(WavHeader)) {
    Serial.println("  ❌ ERROR: Failed to read complete WAV header");
    return false;
  }

  // Validate WAV format
  Serial.print("  DEBUG: RIFF header: ");
  Serial.write((uint8_t*)header.riff, 4);
  Serial.println();
  Serial.print("  DEBUG: WAVE header: ");
  Serial.write((uint8_t*)header.wave, 4);
  Serial.println();

  if (strncmp(header.riff, "RIFF", 4) != 0 ||
      strncmp(header.wave, "WAVE", 4) != 0) {
    Serial.println("  ❌ ERROR: Not a valid WAV file (RIFF/WAVE header mismatch)");
    return false;
  }

  Serial.print("  DEBUG: Audio format: ");
  Serial.println(header.audioFormat);
  if (header.audioFormat != 1) {  // PCM
    Serial.println("  ❌ ERROR: Not PCM format (only PCM supported)");
    return false;
  }

  Serial.print("  DEBUG: Bits per sample: ");
  Serial.println(header.bitsPerSample);
  if (header.bitsPerSample != 16) {
    Serial.println("  ❌ ERROR: Not 16-bit (only 16-bit supported)");
    return false;
  }

  Serial.print("  DEBUG: Channels: ");
  Serial.println(header.numChannels);
  Serial.print("  DEBUG: Sample rate: ");
  Serial.println(header.sampleRate);

  cached->numChannels = header.numChannels;

  // Find data chunk
  Serial.println("  DEBUG: Searching for 'data' chunk...");
  char chunkID[5] = {0};
  uint32_t chunkSize;
  int chunksScanned = 0;

  while (file.available()) {
    chunksScanned++;
    if (file.read((uint8_t*)chunkID, 4) != 4) {
      Serial.println("  ❌ ERROR: Failed to read chunk ID");
      break;
    }
    if (file.read((uint8_t*)&chunkSize, 4) != 4) {
      Serial.println("  ❌ ERROR: Failed to read chunk size");
      break;
    }

    chunkID[4] = '\0';
    Serial.print("  DEBUG: Found chunk '");
    Serial.print(chunkID);
    Serial.print("' size ");
    Serial.print(chunkSize);
    Serial.println(" bytes");

    if (strncmp(chunkID, "data", 4) == 0) {
      Serial.println("  ✓ Found data chunk!");

      // Found data chunk - allocate memory
      uint32_t numSamples = chunkSize / 2;  // 16-bit samples

      Serial.print("  DEBUG: Allocating ");
      Serial.print(chunkSize);
      Serial.print(" bytes (");
      Serial.print(chunkSize / 1024);
      Serial.println(" KB)...");

      // Try to allocate memory - on RP2350 with PSRAM this should use PSRAM automatically
      cached->audioData = (int16_t*)malloc(chunkSize);
      if (cached->audioData == NULL) {
        Serial.println("  ❌ ERROR: malloc() FAILED - out of memory!");
        Serial.println("  ⚠️  Note: Large files may need to be split into smaller chunks");
        return false;
      }

      totalMemoryUsed += chunkSize;
      Serial.print("  ✓ Memory allocated (Total: ");
      Serial.print(totalMemoryUsed / 1024);
      Serial.println(" KB)");

      // Read all audio data to memory
      Serial.println("  DEBUG: Reading audio data from SD...");
      uint32_t bytesRead = file.read((uint8_t*)cached->audioData, chunkSize);
      Serial.print("  DEBUG: Read ");
      Serial.print(bytesRead);
      Serial.print(" / ");
      Serial.print(chunkSize);
      Serial.println(" bytes");

      if (bytesRead == chunkSize) {
        cached->numSamples = numSamples;
        cached->loaded = true;
        Serial.println("  ✓ Audio data loaded successfully!");

        // Calculate and print CRC for verification
        uint32_t crc = 0;
        for (uint32_t i = 0; i < min((uint32_t)1000, numSamples); i++) {
          crc = crc * 31 + cached->audioData[i];
        }
        Serial.print("crc: ");
        Serial.println(crc, HEX);

        return true;
      } else {
        Serial.println("  ❌ ERROR: Failed to read complete audio data");
        free(cached->audioData);
        cached->audioData = NULL;
        return false;
      }
    } else {
      // Skip this chunk
      Serial.print("  DEBUG: Skipping chunk, seeking +");
      Serial.println(chunkSize);
      file.seek(file.position() + chunkSize);
    }
  }

  Serial.print("  ❌ ERROR: 'data' chunk not found after scanning ");
  Serial.print(chunksScanned);
  Serial.println(" chunks");
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

  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (cachedFiles[i].loaded) {
      Serial.print("║ Track ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(cachedFiles[i].filename);

      uint32_t bytes = cachedFiles[i].numSamples * 2;

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
  Serial.print(totalMemoryUsed / 1024);
  Serial.print(" KB / ");
  Serial.print(MAX_TOTAL_MEMORY_KB);
  Serial.print(" KB (");
  Serial.print((totalMemoryUsed * 100) / (MAX_TOTAL_MEMORY_KB * 1024));
  Serial.println("%)");

  Serial.print("║ Per-file limit: ");
  Serial.print(MAX_FILE_SIZE_KB);
  Serial.println(" KB");

  Serial.println("╚════════════════════════════════════════════╝");
}
