// Tone Sweep Example
// Demonstrates AudioSynthToneSweep
// Creates smooth frequency sweeps (useful for measurements and effects)

#include <pico-audio.h>

// Create audio components
AudioSynthToneSweep   sweep1;      // Tone sweep generator
AudioOutputI2S        i2s1;        // I2S output to PCM5102
AudioConnection       patchCord1(sweep1, 0, i2s1, 0);  // Left channel
AudioConnection       patchCord2(sweep1, 0, i2s1, 1);  // Right channel

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(4);

  // Start I2S output
  i2s1.begin();

  Serial.println("Tone Sweep Generator");
  Serial.println("Sweeps from 100 Hz to 4000 Hz");

  // Initial sweep
  sweep1.play(0.5, 100, 4000, 5.0);  // amplitude, startFreq, endFreq, time(seconds)
}

void loop() {
  // Check if sweep is complete and start a new one
  if (!sweep1.isPlaying()) {
    delay(500);  // Pause between sweeps

    // Start new sweep
    sweep1.play(0.5, 100, 4000, 5.0);

    Serial.println("Starting new sweep: 100 Hz -> 4000 Hz over 5 seconds");
  }

  // Print progress every second
  static uint32_t next = millis() + 1000;
  if (millis() >= next) {
    next = millis() + 1000;

    if (sweep1.isPlaying()) {
      Serial.println("Sweeping...");
    }
  }
}
