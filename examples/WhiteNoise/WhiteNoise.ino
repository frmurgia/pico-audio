// White Noise Generator Example
// Demonstrates AudioSynthNoiseWhite
// Generates white noise (all frequencies equally represented)

#include <pico-audio.h>

// Create audio components
AudioSynthNoiseWhite  noise1;      // White noise generator
AudioOutputI2S        i2s1;        // I2S output to PCM5102
AudioConnection       patchCord1(noise1, 0, i2s1, 0);  // Left channel
AudioConnection       patchCord2(noise1, 0, i2s1, 1);  // Right channel

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(4);

  // Configure white noise
  noise1.amplitude(0.3);      // Volume (keep low, noise is loud!)

  // Start I2S output
  i2s1.begin();

  Serial.println("White Noise Generator");
  Serial.println("Volume pulses every 2 seconds");
}

void loop() {
  // Pulse the volume on and off
  static uint32_t next = millis() + 2000;
  static bool on = true;

  if (millis() >= next) {
    next = millis() + 2000;

    if (on) {
      noise1.amplitude(0.0);
      Serial.println("Volume: OFF");
    } else {
      noise1.amplitude(0.3);
      Serial.println("Volume: ON");
    }

    on = !on;
  }
}
