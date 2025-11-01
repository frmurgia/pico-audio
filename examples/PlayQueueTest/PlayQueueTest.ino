// AudioPlayQueue Test
// Tests AudioPlayQueue functionality without SD card
// Generates synthetic audio data to verify queue → I2S path
//
// If you hear audio: AudioPlayQueue works, problem is WAV reading
// If NO audio: AudioPlayQueue issue

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Audio components
AudioPlayQueue    queue1;        // Queue (like WAV player uses)
AudioOutputI2S    i2s1;          // I2S output
AudioConnection   patchCord1(queue1, 0, i2s1, 0);  // Left
AudioConnection   patchCord2(queue1, 0, i2s1, 1);  // Right

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 3000)) {
    delay(100);
  }

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   AudioPlayQueue Test              ║");
  Serial.println("║   Synthetic Data Generation        ║");
  Serial.println("╚════════════════════════════════════╝\n");

  // Allocate audio memory
  AudioMemory(20);

  // Start I2S
  i2s1.begin();

  Serial.println("✓ I2S started");
  Serial.println("✓ AudioPlayQueue initialized");
  Serial.println();
  Serial.println("╔════════════════════════════════════╗");
  Serial.println("║  YOU SHOULD HEAR A TONE NOW!       ║");
  Serial.println("║  Generated via AudioPlayQueue      ║");
  Serial.println("╚════════════════════════════════════╝\n");
  Serial.println("This tests the same path used by WAV player:");
  Serial.println("  Data → AudioPlayQueue → I2S → PCM5102");
  Serial.println();
}

// Phase accumulator for sine wave generation
float phase = 0.0;
float frequency = 440.0;  // A4

void loop() {
  // Check if queue needs data (same as WAV player does)
  int16_t* buffer = queue1.getBuffer();

  if (buffer != NULL) {
    // Generate 128 samples of sine wave (same as WAV player buffer size)
    for (int i = 0; i < 128; i++) {
      // Calculate sine wave sample
      float sample = sin(phase) * 0.5 * 32767;  // 50% volume
      buffer[i] = (int16_t)sample;

      // Increment phase
      phase += 2.0 * PI * frequency / 44100.0;
      if (phase >= 2.0 * PI) {
        phase -= 2.0 * PI;
      }
    }

    // Play buffer (same as WAV player does)
    queue1.playBuffer();

    // Count buffers
    static uint32_t bufferCount = 0;
    bufferCount++;

    // Print status every 1000 buffers (~2.9 seconds)
    if (bufferCount % 1000 == 0) {
      Serial.print("♪ Buffers: ");
      Serial.print(bufferCount);
      Serial.print(" | CPU: ");
      Serial.print(AudioProcessorUsageMax());
      Serial.print("% | Mem: ");
      Serial.print(AudioMemoryUsageMax());
      Serial.println();

      AudioProcessorUsageMaxReset();
    }
  }

  // Very important: small delay to prevent hogging CPU
  // WAV player has this implicitly due to SD reads
  delayMicroseconds(100);
}
