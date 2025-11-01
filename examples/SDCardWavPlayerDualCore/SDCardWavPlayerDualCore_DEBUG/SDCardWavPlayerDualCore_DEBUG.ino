// SD Card WAV Player - DUAL CORE VERSION - DEBUG MODE
// Same as DualCore but with extensive debugging output
// Use this to diagnose why audio doesn't play

#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <SD.h>
#include <pico-audio.h>
#include <pico/multicore.h>
#include <pico/mutex.h>

// Enable debug output
#define DEBUG_BUFFERS 1
#define DEBUG_SAMPLES 1

// SD Card pin configuration
#define SD_CS_PIN   5
#define SD_SCK_PIN  6
#define SD_MOSI_PIN 7
#define SD_MISO_PIN 4

#define NUM_PLAYERS 1  // Test with just 1 player for debugging
#define BUFFER_SIZE 8192

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

struct WavPlayer {
  File file;
  AudioPlayQueue queue;
  volatile bool playing;
  volatile bool stopRequested;
  uint32_t dataSize;
  uint32_t dataPosition;
  uint16_t numChannels;
  char filename[32];

  int16_t* buffer;
  volatile uint32_t bufferSize;
  volatile uint32_t bufferReadPos;
  volatile uint32_t bufferWritePos;
  volatile uint32_t bufferAvailable;

  volatile uint32_t underrunCount;
  volatile uint32_t core1ReadCount;
  volatile uint32_t core0PlayCount;

  mutex_t mutex;
};

WavPlayer players[NUM_PLAYERS];
AudioOutputI2S i2s1;
AudioConnection patchCord1(players[0].queue, 0, i2s1, 0);
AudioConnection patchCord2(players[0].queue, 0, i2s1, 1);

volatile bool core1Running = false;
volatile bool sdInitialized = false;

void core1_main();
void playTrack(int playerIndex);

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  DUAL CORE WAV PLAYER - DEBUG MODE    ║");
  Serial.println("╚════════════════════════════════════════╝");

  AudioMemory(120);

  Serial.println("Initializing player...");
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
    players[i].core0PlayCount = 0;
    snprintf(players[i].filename, 32, "track%d.wav", i + 1);

    players[i].buffer = (int16_t*)malloc(BUFFER_SIZE * sizeof(int16_t));
    if (players[i].buffer == NULL) {
      Serial.println("ERROR: Buffer allocation failed!");
    }
  }

  i2s1.begin();
  Serial.println("✓ I2S started");

  Serial.println("Launching Core1...");
  multicore_launch_core1(core1_main);

  for (int i = 0; i < 50 && !sdInitialized; i++) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (sdInitialized) {
    Serial.println("✓ Core1 and SD initialized");
  } else {
    Serial.println("ERROR: SD init failed!");
  }

  Serial.println("\nReady! Type '1' to play track1.wav");
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == '1') {
      Serial.println("\n>>> Starting playback <<<");
      playTrack(0);
    } else if (cmd == 's') {
      Serial.println(">>> Stopping <<<");
      players[0].stopRequested = true;
    }
  }

  for (int i = 0; i < NUM_PLAYERS; i++) {
    if (players[i].playing) {
      serviceAudioQueue(i);
    }
  }

  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 1000) {
    lastDebug = millis();

    if (players[0].playing) {
      Serial.print("DEBUG | Buf: ");
      Serial.print(players[0].bufferAvailable);
      Serial.print("/");
      Serial.print(BUFFER_SIZE);
      Serial.print(" | Pos: ");
      Serial.print(players[0].dataPosition / 1024);
      Serial.print("K/");
      Serial.print(players[0].dataSize / 1024);
      Serial.print("K | C1reads: ");
      Serial.print(players[0].core1ReadCount);
      Serial.print(" | C0plays: ");
      Serial.print(players[0].core0PlayCount);
      Serial.print(" | Underruns: ");
      Serial.println(players[0].underrunCount);
    }
  }
}

void serviceAudioQueue(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  int16_t* queueBuffer = player->queue.getBuffer();
  if (queueBuffer == NULL) {
    return;
  }

  mutex_enter_blocking(&player->mutex);

  if (player->bufferAvailable < 128) {
    mutex_exit(&player->mutex);

    #ifdef DEBUG_BUFFERS
    Serial.println("UNDERRUN: Buffer empty!");
    #endif

    memset(queueBuffer, 0, 128 * sizeof(int16_t));
    player->queue.playBuffer();
    player->underrunCount++;
    return;
  }

  // Copy from buffer
  for (int i = 0; i < 128; i++) {
    queueBuffer[i] = player->buffer[player->bufferReadPos];
    player->bufferReadPos = (player->bufferReadPos + 1) % BUFFER_SIZE;
  }
  player->bufferAvailable -= 128;
  player->core0PlayCount++;

  #ifdef DEBUG_SAMPLES
  // Print first sample of each buffer
  static uint32_t samplePrintCount = 0;
  if (samplePrintCount++ % 100 == 0) {
    Serial.print("Sample[0]: ");
    Serial.print(queueBuffer[0]);
    Serial.print(" | Sample[64]: ");
    Serial.println(queueBuffer[64]);
  }
  #endif

  mutex_exit(&player->mutex);

  player->queue.playBuffer();
}

void playTrack(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  if (player->playing) {
    player->stopRequested = true;
    delay(100);
  }

  Serial.print("▶ Starting track ");
  Serial.println(playerIndex + 1);

  mutex_enter_blocking(&player->mutex);
  player->playing = true;
  player->stopRequested = false;
  player->bufferReadPos = 0;
  player->bufferWritePos = 0;
  player->bufferAvailable = 0;
  player->underrunCount = 0;
  player->core1ReadCount = 0;
  player->core0PlayCount = 0;
  mutex_exit(&player->mutex);

  // Give Core1 time to open and pre-fill
  Serial.println("Waiting for Core1 to open file...");
  delay(2000);

  Serial.println("Playback should start now!");
}

//=============================================================================
// CORE 1
//=============================================================================

void core1_main() {
  core1Running = true;

  SPI.setRX(SD_MISO_PIN);
  SPI.setTX(SD_MOSI_PIN);
  SPI.setSCK(SD_SCK_PIN);

  if (SD.begin(SD_CS_PIN)) {
    sdInitialized = true;
  } else {
    sdInitialized = false;
    while (1) delay(1000);
  }

  while (true) {
    for (int i = 0; i < NUM_PLAYERS; i++) {
      core1_servicePlayer(i);
    }
    delay(1);
  }
}

void core1_servicePlayer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  if (player->playing && !player->file && !player->stopRequested) {
    core1_openFile(playerIndex);
  }

  if (player->stopRequested && player->file) {
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    player->stopRequested = false;
    mutex_exit(&player->mutex);
    return;
  }

  if (player->playing && player->file) {
    core1_fillBuffer(playerIndex);
  }
}

void core1_openFile(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  Serial.print("Core1: Opening ");
  Serial.println(player->filename);

  player->file = SD.open(player->filename);
  if (!player->file) {
    Serial.println("Core1: File NOT FOUND!");
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    mutex_exit(&player->mutex);
    return;
  }

  Serial.println("Core1: File opened, reading header...");

  WavHeader header;
  player->file.read((uint8_t*)&header, sizeof(WavHeader));

  Serial.print("Core1: RIFF: ");
  Serial.write(header.riff, 4);
  Serial.println();
  Serial.print("Core1: Format: ");
  Serial.println(header.audioFormat);
  Serial.print("Core1: Channels: ");
  Serial.println(header.numChannels);
  Serial.print("Core1: Sample Rate: ");
  Serial.println(header.sampleRate);
  Serial.print("Core1: Bits/Sample: ");
  Serial.println(header.bitsPerSample);

  if (strncmp(header.riff, "RIFF", 4) != 0 ||
      strncmp(header.wave, "WAVE", 4) != 0 ||
      header.audioFormat != 1 ||
      header.bitsPerSample != 16) {
    Serial.println("Core1: INVALID WAV FORMAT!");
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
      Serial.print("Core1: Found data chunk, size: ");
      Serial.print(chunkSize / 1024);
      Serial.println(" KB");
      break;
    }
    player->file.seek(player->file.position() + chunkSize);
  }

  if (!foundData) {
    Serial.println("Core1: NO DATA CHUNK!");
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    mutex_exit(&player->mutex);
    return;
  }

  mutex_enter_blocking(&player->mutex);
  player->dataSize = chunkSize;
  player->dataPosition = 0;
  player->numChannels = header.numChannels;
  mutex_exit(&player->mutex);

  Serial.println("Core1: Pre-filling buffer...");
  for (int i = 0; i < 4; i++) {
    core1_fillBuffer(playerIndex);
  }

  Serial.println("Core1: File ready for playback!");
}

void core1_fillBuffer(int playerIndex) {
  WavPlayer* player = &players[playerIndex];

  mutex_enter_blocking(&player->mutex);
  uint32_t available = player->bufferAvailable;
  uint32_t writePos = player->bufferWritePos;
  mutex_exit(&player->mutex);

  if (available > (BUFFER_SIZE * 3 / 4)) {
    return;
  }

  uint32_t spaceAvailable = BUFFER_SIZE - available;
  uint32_t bytesRemaining = player->dataSize - player->dataPosition;

  if (bytesRemaining == 0) {
    Serial.println("Core1: End of file");
    player->file.close();
    mutex_enter_blocking(&player->mutex);
    player->playing = false;
    mutex_exit(&player->mutex);
    return;
  }

  uint32_t samplesToRead = min(spaceAvailable, (uint32_t)1024);
  uint32_t bytesToRead = min(samplesToRead * 2, bytesRemaining);
  samplesToRead = bytesToRead / 2;

  static uint32_t readCount = 0;
  if (readCount++ % 10 == 0) {
    Serial.print("Core1: Reading ");
    Serial.print(samplesToRead);
    Serial.println(" samples");
  }

  if (player->numChannels == 1) {
    for (uint32_t i = 0; i < samplesToRead; i++) {
      int16_t sample;
      player->file.read((uint8_t*)&sample, 2);
      player->buffer[writePos] = sample;
      writePos = (writePos + 1) % BUFFER_SIZE;

      // Print first sample
      if (i == 0 && readCount % 10 == 0) {
        Serial.print("Core1: First sample value: ");
        Serial.println(sample);
      }
    }
    player->dataPosition += bytesToRead;
  } else {
    for (uint32_t i = 0; i < samplesToRead; i++) {
      int16_t left, right;
      player->file.read((uint8_t*)&left, 2);
      player->file.read((uint8_t*)&right, 2);
      player->buffer[writePos] = ((int32_t)left + (int32_t)right) / 2;
      writePos = (writePos + 1) % BUFFER_SIZE;
    }
    player->dataPosition += (bytesToRead * 2);
  }

  mutex_enter_blocking(&player->mutex);
  player->bufferWritePos = writePos;
  player->bufferAvailable += samplesToRead;
  player->core1ReadCount++;
  mutex_exit(&player->mutex);
}
