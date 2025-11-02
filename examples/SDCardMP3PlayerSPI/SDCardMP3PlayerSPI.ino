// SD Card MP3 Player - SPI DEBUG VERSION
// VERSION: 1.0 DEBUG
// DATE: 2025-11-02
//
// Versione SEMPLIFICATA con SPI per debugging completo
// Verifica step-by-step:
// 1. SD Card initialization
// 2. File listing
// 3. File reading
// 4. MP3 decoding
// 5. Audio output
//
// I2S Pin Configuration (PCM5102 DAC):
// - BCK (Bit Clock)   -> GP20
// - LRCK (Word Select)-> GP21
// - DIN (Data)        -> GP22
//
// SPI SD Card Configuration:
// - CS   -> GP3
// - SCK  -> GP6
// - MOSI -> GP7
// - MISO -> GP0
//
// Comandi Serial:
// - '1'-'9','0' : Play track 1-10
// - 's' : Stop
// - 'l' : List files
// - 'i' : SD info
// - 't' : Test MP3 decode

#include <Adafruit_TinyUSB.h>
#include <SD.h>
#include <SPI.h>
#include <pico-audio.h>

// Include minimp3 decoder
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "minimp3.h"

// SPI Pin Configuration
#define SD_CS_PIN   3
#define SD_SCK_PIN  6
#define SD_MOSI_PIN 7
#define SD_MISO_PIN 0

// Single player for debugging
#define BUFFER_SIZE 8192
#define MP3_BUF_SIZE 4096

// Player state
struct {
  File file;
  AudioPlayQueue queue;
  mp3dec_t mp3dec;

  uint8_t mp3Buffer[MP3_BUF_SIZE];
  uint32_t mp3BufferFill;

  int16_t circularBuffer[BUFFER_SIZE];
  uint32_t bufferReadPos;
  uint32_t bufferWritePos;
  uint32_t bufferAvailable;

  bool playing;
  uint32_t fileSize;
  uint32_t filePosition;
  uint32_t framesDecoded;
  uint32_t samplesGenerated;

  char filename[32];
} player;

// Audio output
AudioOutputI2S i2s1;
AudioConnection patchCord1(player.queue, 0, i2s1, 0);
AudioConnection patchCord2(player.queue, 0, i2s1, 1);

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println("\n╔═══════════════════════════════════════════╗");
  Serial.println("║  SD MP3 Player - SPI DEBUG VERSION       ║");
  Serial.println("║  v1.0 - Diagnostic Mode                  ║");
  Serial.println("╚═══════════════════════════════════════════╝\n");

  // Step 1: Initialize SPI
  Serial.println("╔═══ STEP 1: SPI INITIALIZATION ═══╗");
  Serial.print("  Configuring SPI... ");

  SPI.setRX(SD_MISO_PIN);
  SPI.setTX(SD_MOSI_PIN);
  SPI.setSCK(SD_SCK_PIN);

  Serial.println("OK");
  Serial.print("  MISO: GP"); Serial.println(SD_MISO_PIN);
  Serial.print("  MOSI: GP"); Serial.println(SD_MOSI_PIN);
  Serial.print("  SCK:  GP"); Serial.println(SD_SCK_PIN);
  Serial.print("  CS:   GP"); Serial.println(SD_CS_PIN);
  Serial.println();

  // Step 2: Initialize SD Card
  Serial.println("╔═══ STEP 2: SD CARD INITIALIZATION ═══╗");
  Serial.print("  Initializing SD card... ");

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("FAILED!");
    Serial.println("  ❌ Cannot initialize SD card");
    Serial.println("\n  Troubleshooting:");
    Serial.println("  1. Check SD card is inserted");
    Serial.println("  2. Check wiring:");
    Serial.println("     CS:   GP3");
    Serial.println("     SCK:  GP6");
    Serial.println("     MOSI: GP7");
    Serial.println("     MISO: GP0");
    Serial.println("  3. Try different SD card");
    Serial.println("  4. Format SD as FAT32");
    while (1) delay(1000);
  }

  Serial.println("OK!");

  // Get SD card info
  uint64_t cardSize = SD.size();
  uint32_t cardSizeMB = cardSize / (1024 * 1024);

  Serial.print("  ✓ Card size: ");
  Serial.print(cardSizeMB);
  Serial.println(" MB");

  // Try to detect card type
  Serial.print("  ✓ Card type: ");
  if (cardSize < 2ULL * 1024 * 1024 * 1024) {
    Serial.println("SD/SDHC (< 2GB)");
  } else if (cardSize < 32ULL * 1024 * 1024 * 1024) {
    Serial.println("SDHC (2-32GB)");
  } else {
    Serial.println("SDXC (> 32GB)");
  }
  Serial.println();

  // Step 3: List files
  Serial.println("╔═══ STEP 3: FILE LISTING ═══╗");
  listFiles();
  Serial.println();

  // Step 4: Initialize Audio
  Serial.println("╔═══ STEP 4: AUDIO INITIALIZATION ═══╗");
  Serial.print("  Allocating audio memory... ");
  AudioMemory(40);
  Serial.println("OK (40 blocks)");

  Serial.print("  Initializing I2S output... ");
  i2s1.begin();
  Serial.println("OK");
  Serial.println("  I2S Pins:");
  Serial.println("    BCK:  GP20");
  Serial.println("    LRCK: GP21");
  Serial.println("    DIN:  GP22");
  Serial.println();

  // Step 5: Initialize MP3 decoder
  Serial.println("╔═══ STEP 5: MP3 DECODER INITIALIZATION ═══╗");
  Serial.print("  Initializing minimp3... ");
  mp3dec_init(&player.mp3dec);
  Serial.println("OK");
  Serial.print("  MP3 buffer size: ");
  Serial.print(MP3_BUF_SIZE);
  Serial.println(" bytes");
  Serial.print("  Audio buffer size: ");
  Serial.print(BUFFER_SIZE);
  Serial.println(" samples");
  Serial.println();

  // Initialize player state
  player.playing = false;
  player.bufferReadPos = 0;
  player.bufferWritePos = 0;
  player.bufferAvailable = 0;
  player.mp3BufferFill = 0;
  player.framesDecoded = 0;
  player.samplesGenerated = 0;

  Serial.println("╔═══════════════════════════════════════════╗");
  Serial.println("║  READY FOR TESTING                        ║");
  Serial.println("╚═══════════════════════════════════════════╝\n");

  Serial.println("Commands:");
  Serial.println("  '1'-'9','0' : Play track 1-10");
  Serial.println("  's' : Stop playback");
  Serial.println("  'l' : List files again");
  Serial.println("  'i' : SD card info");
  Serial.println("  't' : Test MP3 decode (first file)");
  Serial.println("  'd' : Show debug info");
  Serial.println();
}

void loop() {
  // Handle commands
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd >= '1' && cmd <= '9') {
      playTrack(cmd - '1');
    } else if (cmd == '0') {
      playTrack(9);
    } else if (cmd == 's' || cmd == 'S') {
      stopPlayback();
    } else if (cmd == 'l' || cmd == 'L') {
      listFiles();
    } else if (cmd == 'i' || cmd == 'I') {
      showSDInfo();
    } else if (cmd == 't' || cmd == 'T') {
      testMP3Decode();
    } else if (cmd == 'd' || cmd == 'D') {
      showDebugInfo();
    }
  }

  // Service audio if playing
  if (player.playing) {
    serviceAudio();
  }

  delay(1);
}

void listFiles() {
  Serial.println("  Files on SD card:");

  File root = SD.open("/");
  if (!root) {
    Serial.println("  ❌ Cannot open root directory");
    return;
  }

  int fileCount = 0;
  int mp3Count = 0;

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      fileCount++;
      Serial.print("    ");
      Serial.print(entry.name());
      Serial.print(" (");
      Serial.print(entry.size());
      Serial.print(" bytes)");

      // Check if MP3
      String name = String(entry.name());
      name.toLowerCase();
      if (name.endsWith(".mp3")) {
        Serial.print(" ✓ MP3");
        mp3Count++;
      }
      Serial.println();
    }
    entry.close();
  }
  root.close();

  Serial.print("\n  Total files: ");
  Serial.print(fileCount);
  Serial.print(" (");
  Serial.print(mp3Count);
  Serial.println(" MP3 files)");

  if (mp3Count == 0) {
    Serial.println("\n  ⚠️  No MP3 files found!");
    Serial.println("  Please add files: track1.mp3, track2.mp3, etc.");
  }
}

void showSDInfo() {
  Serial.println("\n╔═══ SD CARD INFO ═══╗");

  uint64_t cardSize = SD.size();
  uint64_t totalBytes = cardSize;
  uint64_t usedBytes = 0;

  // Count used space
  File root = SD.open("/");
  if (root) {
    while (true) {
      File entry = root.openNextFile();
      if (!entry) break;
      if (!entry.isDirectory()) {
        usedBytes += entry.size();
      }
      entry.close();
    }
    root.close();
  }

  Serial.print("  Total: ");
  Serial.print((uint32_t)(totalBytes / (1024 * 1024)));
  Serial.println(" MB");

  Serial.print("  Used:  ");
  Serial.print((uint32_t)(usedBytes / (1024 * 1024)));
  Serial.println(" MB");

  Serial.print("  Free:  ");
  Serial.print((uint32_t)((totalBytes - usedBytes) / (1024 * 1024)));
  Serial.println(" MB");

  Serial.println();
}

void testMP3Decode() {
  Serial.println("\n╔═══ MP3 DECODE TEST ═══╗");

  // Try to find first MP3 file
  File root = SD.open("/");
  if (!root) {
    Serial.println("  ❌ Cannot open root");
    return;
  }

  File mp3File;
  String mp3Name;

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      String name = String(entry.name());
      name.toLowerCase();
      if (name.endsWith(".mp3")) {
        mp3Name = String(entry.name());
        entry.close();
        break;
      }
    }
    entry.close();
  }
  root.close();

  if (mp3Name.length() == 0) {
    Serial.println("  ❌ No MP3 files found");
    return;
  }

  Serial.print("  Testing file: ");
  Serial.println(mp3Name);

  mp3File = SD.open(mp3Name.c_str(), FILE_READ);
  if (!mp3File) {
    Serial.println("  ❌ Cannot open file");
    return;
  }

  Serial.print("  File size: ");
  Serial.print(mp3File.size());
  Serial.println(" bytes");

  // Read first chunk
  uint8_t testBuffer[4096];
  size_t bytesRead = mp3File.read(testBuffer, 4096);

  Serial.print("  Read ");
  Serial.print(bytesRead);
  Serial.println(" bytes");

  // Show first bytes (check for ID3 tag)
  Serial.print("  First bytes: ");
  for (int i = 0; i < 16 && i < bytesRead; i++) {
    if (testBuffer[i] < 16) Serial.print("0");
    Serial.print(testBuffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Check for ID3 tag
  if (bytesRead >= 3 && testBuffer[0] == 'I' && testBuffer[1] == 'D' && testBuffer[2] == '3') {
    Serial.println("  ✓ ID3 tag detected");
  } else if (bytesRead >= 2 && testBuffer[0] == 0xFF && (testBuffer[1] & 0xE0) == 0xE0) {
    Serial.println("  ✓ MP3 frame sync detected");
  } else {
    Serial.println("  ⚠️  Unknown format");
  }

  // Try to decode first frame
  mp3dec_t testDecoder;
  mp3dec_init(&testDecoder);

  int16_t pcmBuffer[MINIMP3_MAX_SAMPLES_PER_FRAME];
  mp3dec_frame_info_t frameInfo;

  Serial.print("\n  Attempting decode... ");
  int samples = mp3dec_decode_frame(&testDecoder, testBuffer, bytesRead, pcmBuffer, &frameInfo);

  if (samples > 0) {
    Serial.println("SUCCESS!");
    Serial.print("    Samples: ");
    Serial.println(samples);
    Serial.print("    Sample rate: ");
    Serial.print(frameInfo.hz);
    Serial.println(" Hz");
    Serial.print("    Channels: ");
    Serial.println(frameInfo.channels);
    Serial.print("    Bitrate: ");
    Serial.print(frameInfo.bitrate_kbps);
    Serial.println(" kbps");
    Serial.print("    Frame size: ");
    Serial.print(frameInfo.frame_bytes);
    Serial.println(" bytes");

    // Show first few samples
    Serial.print("    First samples: ");
    for (int i = 0; i < 8 && i < samples; i++) {
      Serial.print(pcmBuffer[i]);
      Serial.print(" ");
    }
    Serial.println();

  } else {
    Serial.println("FAILED");
    Serial.print("    Error code: ");
    Serial.println(samples);
    Serial.println("    Possible reasons:");
    Serial.println("    - ID3 tag needs to be skipped");
    Serial.println("    - Corrupted file");
    Serial.println("    - Unsupported format");
  }

  mp3File.close();
  Serial.println();
}

void playTrack(int trackNum) {
  if (player.playing) {
    stopPlayback();
    delay(100);
  }

  // Build filename
  snprintf(player.filename, sizeof(player.filename), "track%d.mp3", trackNum + 1);

  Serial.print("\n▶ Playing: ");
  Serial.println(player.filename);

  // Open file
  player.file = SD.open(player.filename, FILE_READ);
  if (!player.file) {
    Serial.println("  ❌ File not found!");
    Serial.print("  Expected: ");
    Serial.println(player.filename);
    return;
  }

  player.fileSize = player.file.size();
  player.filePosition = 0;

  Serial.print("  File size: ");
  Serial.print(player.fileSize);
  Serial.println(" bytes");

  // Reset buffers
  player.bufferReadPos = 0;
  player.bufferWritePos = 0;
  player.bufferAvailable = 0;
  player.mp3BufferFill = 0;
  player.framesDecoded = 0;
  player.samplesGenerated = 0;

  // Reinit decoder
  mp3dec_init(&player.mp3dec);

  player.playing = true;

  Serial.println("  ✓ Started");
}

void stopPlayback() {
  if (!player.playing) return;

  Serial.println("\n■ Stopping playback");

  player.playing = false;

  if (player.file) {
    player.file.close();
  }

  Serial.println("  ✓ Stopped");
}

void serviceAudio() {
  // Fill buffer from SD + decode
  fillBufferFromMP3();

  // Send to audio queue
  sendToQueue();

  // Stats every 2 seconds
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 2000) {
    lastStats = millis();

    Serial.print("♪ Playing | Buf: ");
    Serial.print((player.bufferAvailable * 100) / BUFFER_SIZE);
    Serial.print("% | Frames: ");
    Serial.print(player.framesDecoded);
    Serial.print(" | Samples: ");
    Serial.print(player.samplesGenerated);
    Serial.print(" | Pos: ");
    Serial.print(player.filePosition);
    Serial.print("/");
    Serial.print(player.fileSize);
    Serial.print(" (");
    Serial.print((player.filePosition * 100) / player.fileSize);
    Serial.println("%)");
  }
}

void fillBufferFromMP3() {
  // Don't overfill
  if (player.bufferAvailable > (BUFFER_SIZE * 3 / 4)) {
    return;
  }

  // Refill MP3 buffer if needed
  if (player.mp3BufferFill < MP3_BUF_SIZE / 2 && player.file.available()) {
    // Shift remaining data
    if (player.mp3BufferFill > 0) {
      memmove(player.mp3Buffer,
              player.mp3Buffer + (MP3_BUF_SIZE - player.mp3BufferFill),
              player.mp3BufferFill);
    }

    // Read more
    uint32_t toRead = MP3_BUF_SIZE - player.mp3BufferFill;
    uint32_t bytesRead = player.file.read(player.mp3Buffer + player.mp3BufferFill, toRead);
    player.mp3BufferFill += bytesRead;
    player.filePosition += bytesRead;
  }

  // Decode one frame
  if (player.mp3BufferFill > 0) {
    int16_t pcmBuffer[MINIMP3_MAX_SAMPLES_PER_FRAME];
    mp3dec_frame_info_t frameInfo;

    int samples = mp3dec_decode_frame(&player.mp3dec,
                                       player.mp3Buffer,
                                       player.mp3BufferFill,
                                       pcmBuffer,
                                       &frameInfo);

    if (samples > 0) {
      player.framesDecoded++;

      // Convert stereo to mono if needed
      int monoSamples = samples;
      if (frameInfo.channels == 2) {
        monoSamples = samples / 2;
        for (int i = 0; i < monoSamples; i++) {
          int32_t left = pcmBuffer[i * 2];
          int32_t right = pcmBuffer[i * 2 + 1];
          pcmBuffer[i] = (int16_t)((left + right) / 2);
        }
      }

      // Add to circular buffer
      for (int i = 0; i < monoSamples && player.bufferAvailable < BUFFER_SIZE; i++) {
        player.circularBuffer[player.bufferWritePos] = pcmBuffer[i];
        player.bufferWritePos = (player.bufferWritePos + 1) % BUFFER_SIZE;
        player.bufferAvailable++;
        player.samplesGenerated++;
      }

      // Remove decoded data
      if (frameInfo.frame_bytes > 0) {
        player.mp3BufferFill -= frameInfo.frame_bytes;
        if (player.mp3BufferFill > 0) {
          memmove(player.mp3Buffer,
                  player.mp3Buffer + frameInfo.frame_bytes,
                  player.mp3BufferFill);
        }
      }
    } else {
      // Decode failed - skip byte for resync
      if (player.mp3BufferFill > 0) {
        player.mp3BufferFill--;
        memmove(player.mp3Buffer, player.mp3Buffer + 1, player.mp3BufferFill);
      }

      // Check end of file
      if (!player.file.available() && player.mp3BufferFill < 128) {
        Serial.println("\n✓ Playback finished");
        stopPlayback();
      }
    }
  }
}

void sendToQueue() {
  int16_t* queueBuffer = player.queue.getBuffer();
  if (queueBuffer == NULL) {
    return;  // Queue full
  }

  if (player.bufferAvailable < 128) {
    // Underrun - send silence
    memset(queueBuffer, 0, 128 * sizeof(int16_t));
    player.queue.playBuffer();
    return;
  }

  // Copy from circular buffer
  for (int i = 0; i < 128; i++) {
    queueBuffer[i] = player.circularBuffer[player.bufferReadPos];
    player.bufferReadPos = (player.bufferReadPos + 1) % BUFFER_SIZE;
  }
  player.bufferAvailable -= 128;

  player.queue.playBuffer();
}

void showDebugInfo() {
  Serial.println("\n╔═══ DEBUG INFO ═══╗");
  Serial.print("  Playing: ");
  Serial.println(player.playing ? "YES" : "NO");

  if (player.playing) {
    Serial.print("  File: ");
    Serial.println(player.filename);
    Serial.print("  Position: ");
    Serial.print(player.filePosition);
    Serial.print(" / ");
    Serial.print(player.fileSize);
    Serial.print(" (");
    Serial.print((player.filePosition * 100) / player.fileSize);
    Serial.println("%)");

    Serial.print("  MP3 buffer: ");
    Serial.print(player.mp3BufferFill);
    Serial.print(" / ");
    Serial.print(MP3_BUF_SIZE);
    Serial.println(" bytes");

    Serial.print("  Audio buffer: ");
    Serial.print(player.bufferAvailable);
    Serial.print(" / ");
    Serial.print(BUFFER_SIZE);
    Serial.print(" samples (");
    Serial.print((player.bufferAvailable * 100) / BUFFER_SIZE);
    Serial.println("%)");

    Serial.print("  Frames decoded: ");
    Serial.println(player.framesDecoded);

    Serial.print("  Samples generated: ");
    Serial.println(player.samplesGenerated);
  }

  Serial.print("\n  Audio Memory: ");
  Serial.print(AudioMemoryUsageMax());
  Serial.println(" blocks");

  Serial.print("  CPU Usage: ");
  Serial.print(AudioProcessorUsageMax());
  Serial.println("%");

  Serial.println();
}
