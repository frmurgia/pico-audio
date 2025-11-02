// SD Card WAV Player - Using AudioPlaySdWav (SIMPLE VERSION)
// VERSION: 1.0 (2025-11-02)
//
// Based on Teensy AudioPlaySdWav class - much simpler than dual-core approach!
//
// Benefits:
// - No dual-core complexity
// - No mutex/threading issues
// - Proven Teensy code architecture
// - Simple state machine in update()
//
// I2S Pin Configuration (PCM5102 DAC):
// - BCK (Bit Clock)   -> GP20
// - LRCK (Word Select)-> GP21
// - DIN (Data)        -> GP22
// - VCC               -> 3.3V
// - GND               -> GND
//
// SDIO Pin Configuration (6 pins required):
// - SD_CLK  -> GP6  (clock)
// - SD_CMD  -> GP7  (command)
// - SD_DAT0 -> GP8  (data bit 0, must be base for consecutive pins)
// - SD_DAT1 -> GP9  (data bit 1, must be DAT0+1)
// - SD_DAT2 -> GP10 (data bit 2, must be DAT0+2)
// - SD_DAT3 -> GP11 (data bit 3, must be DAT0+3)
//
// Controls via Serial:
// - '1'-'9','0' : Play track 1-10
// - 's' : Stop
// - 'l' : List files
//

#include <Adafruit_TinyUSB.h>
#include <SD.h>
#include <pico-audio.h>

// SDIO Pin Configuration for RP2350
#define SD_CLK_PIN  6
#define SD_CMD_PIN  7
#define SD_DAT0_PIN 8   // DAT1=9, DAT2=10, DAT3=11 (consecutive!)

// Number of simultaneous players
#define NUM_PLAYERS 10

// Audio components
AudioPlaySdWav players[NUM_PLAYERS];
AudioMixer4 mixer1, mixer2, mixer3;
AudioOutputI2S i2s1;

// Audio connections
AudioConnection patchCord1(players[0], 0, mixer1, 0);
AudioConnection patchCord2(players[0], 1, mixer1, 1);
AudioConnection patchCord3(players[1], 0, mixer1, 2);
AudioConnection patchCord4(players[1], 1, mixer1, 3);

AudioConnection patchCord5(players[2], 0, mixer2, 0);
AudioConnection patchCord6(players[2], 1, mixer2, 1);
AudioConnection patchCord7(players[3], 0, mixer2, 2);
AudioConnection patchCord8(players[3], 1, mixer2, 3);

AudioConnection patchCord9(players[4], 0, mixer3, 0);
AudioConnection patchCord10(players[4], 1, mixer3, 1);
AudioConnection patchCord11(mixer1, 0, mixer3, 2);
AudioConnection patchCord12(mixer2, 0, mixer3, 3);

AudioConnection patchCord13(mixer3, 0, i2s1, 0);
AudioConnection patchCord14(mixer3, 0, i2s1, 1);

// Global state
float globalVolume = 0.3;  // 30% per channel
bool sdInitialized = false;

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  SD WAV Player - AudioPlaySdWav       ║");
  Serial.println("║  VERSION 1.0 (2025-11-02)             ║");
  Serial.println("║  RP2350B - SDIO 4-bit Mode            ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println();

  // Initialize SD card in SDIO mode
  Serial.print("Initializing SD card (SDIO)... ");
  if (SD.begin(SD_CLK_PIN, SD_CMD_PIN, SD_DAT0_PIN)) {
    Serial.println("OK");

    uint64_t cardSize = SD.size();
    uint32_t cardSizeMB = cardSize / (1024 * 1024);
    Serial.print("SD card size: ");
    Serial.print(cardSizeMB);
    Serial.println(" MB");

    sdInitialized = true;
  } else {
    Serial.println("FAILED");
    Serial.println("Check SDIO wiring:");
    Serial.println("  CLK:  GP6");
    Serial.println("  CMD:  GP7");
    Serial.println("  DAT0: GP8");
    Serial.println("  DAT1: GP9");
    Serial.println("  DAT2: GP10");
    Serial.println("  DAT3: GP11");
  }

  // Initialize audio
  AudioMemory(120);

  // Configure mixers
  updateMixerGains();

  // Initialize I2S
  i2s1.begin();

  Serial.println();
  Serial.println("Ready!");
  Serial.println("Commands: '1'-'0' = play track, 's' = stop, 'l' = list");
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
      showFileList();
    }
  }

  // Print stats every 2 seconds
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 2000) {
    lastStats = millis();

    int activePlayers = 0;
    for (int i = 0; i < NUM_PLAYERS; i++) {
      if (players[i].isPlaying()) {
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

void playTrack(int playerIndex) {
  if (playerIndex < 0 || playerIndex >= NUM_PLAYERS) return;
  if (!sdInitialized) {
    Serial.println("⚠️  SD card not initialized!");
    return;
  }

  // Build filename
  char filename[32];
  snprintf(filename, sizeof(filename), "track%d.wav", playerIndex + 1);

  Serial.print("▶ Loading ");
  Serial.println(filename);

  if (players[playerIndex].play(filename)) {
    Serial.println("✓ Started");
  } else {
    Serial.println("❌ Failed to open file");
  }
}

void stopAll() {
  Serial.println("■ Stopping all tracks");
  for (int i = 0; i < NUM_PLAYERS; i++) {
    players[i].stop();
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
    Serial.println("║    Player looks for: track1.wav, track2.wav, etc.");
  }

  Serial.println("╚════════════════════════════════════════════╝");
}
