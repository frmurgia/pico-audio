// SD Card WAV Player - Multi-File Playback
// Plays up to 10 WAV files simultaneously from SD card
//
// SD Card Wiring (SPI):
// - SCK (Clock)  -> GP6
// - MOSI (Data In) -> GP7
// - MISO (Data Out) -> GP4
// - CS (Chip Select) -> GP5
//
// Prepare SD card with WAV files:
// - Format as FAT16 or FAT32
// - Copy WAV files: track1.wav, track2.wav, etc.
// - WAV format: 16-bit, 44100 Hz, mono or stereo
//
// Controls via Serial:
// - '1'-'9','0' : Play track 1-10
// - 's' : Stop all tracks
// - 'l' : List available WAV files

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

// WAV file header structure
struct WavHeader {
  char riff[4];          // "RIFF"
  uint32_t fileSize;
  char wave[4];          // "WAVE"
  char fmt[4];           // "fmt "
  uint32_t fmtSize;
  uint16_t audioFormat;  // 1 = PCM
  uint16_t numChannels;  // 1 = mono, 2 = stereo
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
};

// Player state
struct WavPlayer {
  File file;
  AudioPlayQueue queue;
  bool playing;
  uint32_t dataSize;
  uint32_t dataPosition;
  uint16_t numChannels;
  char filename[32];
};

// Audio components
WavPlayer players[NUM_PLAYERS];
AudioMixer4 mixer1, mixer2, mixer3;  // Three mixers to handle 10 inputs
AudioOutputI2S i2s1;

// Audio connections
// Players 0-3 -> mixer1
AudioConnection patchCord1(players[0].queue, 0, mixer1, 0);
AudioConnection patchCord2(players[1].queue, 0, mixer1, 1);
AudioConnection patchCord3(players[2].queue, 0, mixer1, 2);
AudioConnection patchCord4(players[3].queue, 0, mixer1, 3);

// Players 4-7 -> mixer2
AudioConnection patchCord5(players[4].queue, 0, mixer2, 0);
AudioConnection patchCord6(players[5].queue, 0, mixer2, 1);
AudioConnection patchCord7(players[6].queue, 0, mixer2, 2);
AudioConnection patchCord8(players[7].queue, 0, mixer2, 3);

// Players 8-9 -> mixer3 (channels 0-1)
AudioConnection patchCord9(players[8].queue, 0, mixer3, 0);
AudioConnection patchCord10(players[9].queue, 0, mixer3, 1);

// Mix all mixer outputs to mixer3
AudioConnection patchCord11(mixer1, 0, mixer3, 2);
AudioConnection patchCord12(mixer2, 0, mixer3, 3);

// Final output
AudioConnection patchCord13(mixer3, 0, i2s1, 0);  // Left
AudioConnection patchCord14(mixer3, 0, i2s1, 1);  // Right

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println("\n=== SD Card WAV Player ===");
  Serial.println("Multi-file playback (up to 10 simultaneous)");

  // Configure SPI pins for SD
  SPI.setRX(SD_MISO_PIN);
  SPI.setTX(SD_MOSI_PIN);
  SPI.setSCK(SD_SCK_PIN);

  // Initialize SD card
  Serial.print("Initializing SD card...");
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(" FAILED!");
    Serial.println("Cannot continue without SD card");
    while (1) delay(1000);
  }
  Serial.println(" OK");

  // Allocate audio memory (need more for multiple players)
  AudioMemory(100);

  // Configure mixers - reduce gain to prevent clipping
  float gain = 0.1;  // 10% per channel (10 channels = 100% max)
  for (int i = 0; i < 4; i++) {
    mixer1.gain(i, gain);
    mixer2.gain(i, gain);
    mixer3.gain(i, gain);
  }

  // Initialize players
  for (int i = 0; i < NUM_PLAYERS; i++) {
    players[i].playing = false;
    players[i].dataSize = 0;
    players[i].dataPosition = 0;
    players[i].numChannels = 1;
    snprintf(players[i].filename, 32, "track%d.wav", i + 1);
  }

  // Start I2S
  i2s1.begin();

  Serial.println("\nSD Card WAV Player Ready!");
  Serial.println("\nCommands:");
  Serial.println("  1-9, 0 : Play track 1-10");
  Serial.println("  s      : Stop all");
  Serial.println("  l      : List WAV files");
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
      playTrack(9);  // Track 10
    } else if (cmd == 's' || cmd == 'S') {
      stopAll();
    } else if (cmd == 'l' || cmd == 'L') {
      listWavFiles();
    }
  }

  // Service all active players
  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (players[i].playing) {
      servicePlayer(i);
    }
  }

  // Print stats every 2 seconds
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 2000) {
    lastStats = millis();

    int activePlayers = 0;
    for (int i = 0; i < NUM_PLAYERS; i++) {
      if (players[i].playing) activePlayers++;
    }

    if (activePlayers > 0) {
      Serial.print("Active players: ");
      Serial.print(activePlayers);
      Serial.print(" | CPU: ");
      Serial.print(AudioProcessorUsageMax());
      Serial.print("% | Memory: ");
      Serial.println(AudioMemoryUsageMax());
      AudioProcessorUsageMaxReset();
    }
  }
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

  // Read and validate WAV header
  WavHeader header;
  player->file.read(&header, sizeof(WavHeader));

  if (strncmp(header.riff, "RIFF", 4) != 0 ||
      strncmp(header.wave, "WAVE", 4) != 0) {
    Serial.println(" INVALID WAV FILE");
    player->file.close();
    return;
  }

  if (header.audioFormat != 1) {
    Serial.println(" NOT PCM FORMAT");
    player->file.close();
    return;
  }

  if (header.sampleRate != 44100) {
    Serial.print(" WARNING: Sample rate is ");
    Serial.print(header.sampleRate);
    Serial.println(" Hz (expected 44100)");
  }

  if (header.bitsPerSample != 16) {
    Serial.println(" UNSUPPORTED BIT DEPTH");
    player->file.close();
    return;
  }

  // Find data chunk
  char chunkID[4];
  uint32_t chunkSize;
  bool foundData = false;

  while (player->file.available()) {
    player->file.read(chunkID, 4);
    player->file.read(&chunkSize, 4);

    if (strncmp(chunkID, "data", 4) == 0) {
      foundData = true;
      break;
    }

    // Skip this chunk
    player->file.seek(player->file.position() + chunkSize);
  }

  if (!foundData) {
    Serial.println(" NO DATA CHUNK");
    player->file.close();
    return;
  }

  player->dataSize = chunkSize;
  player->dataPosition = 0;
  player->numChannels = header.numChannels;
  player->playing = true;

  Serial.print(" OK (");
  Serial.print(player->dataSize / 1024);
  Serial.print(" KB, ");
  Serial.print(header.numChannels == 1 ? "mono" : "stereo");
  Serial.println(")");
}

void servicePlayer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  // Check if player needs more data
  int16_t* buffer = player->queue.getBuffer();
  if (buffer == NULL) return;  // Queue is full

  // Read audio data
  int samplesToRead = 128;  // One audio block
  int bytesToRead = samplesToRead * 2;  // 16-bit samples

  if (player->dataPosition + bytesToRead > player->dataSize) {
    // End of file
    memset(buffer, 0, 128 * sizeof(int16_t));
    player->queue.playBuffer();
    stopPlayer(playerIndex);
    return;
  }

  if (player->numChannels == 1) {
    // Mono - read directly
    player->file.read(buffer, bytesToRead);
  } else {
    // Stereo - mix to mono by averaging channels
    int16_t stereoBuffer[256];
    player->file.read(stereoBuffer, bytesToRead * 2);

    for (int i = 0; i < samplesToRead; i++) {
      int32_t left = stereoBuffer[i * 2];
      int32_t right = stereoBuffer[i * 2 + 1];
      buffer[i] = (left + right) / 2;
    }
  }

  player->dataPosition += bytesToRead * player->numChannels;
  player->queue.playBuffer();
}

void stopPlayer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  if (player->playing) {
    player->file.close();
    player->playing = false;
    player->dataPosition = 0;

    Serial.print("Stopped player ");
    Serial.println(playerIndex + 1);
  }
}

void stopAll() {
  Serial.println("Stopping all players...");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    stopPlayer(i);
  }
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

      // Check for .wav extension
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
    Serial.println("  Copy WAV files to SD card:");
    Serial.println("  - track1.wav, track2.wav, etc.");
    Serial.println("  - Format: 16-bit, 44100 Hz");
  } else {
    Serial.print("\nFound ");
    Serial.print(wavCount);
    Serial.println(" WAV file(s)");
  }

  Serial.println();
}
