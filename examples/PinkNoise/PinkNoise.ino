// Pink Noise Generator Example
// Demonstrates AudioSynthNoisePink
// Generates pink noise (1/f noise, more natural sounding than white noise)

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthNoisePink   pinkNoise1;    // Pink noise generator
AudioOutputI2S        i2s1;          // I2S output to PCM5102
AudioConnection       patchCord1(pinkNoise1, 0, i2s1, 0);  // Left channel
AudioConnection       patchCord2(pinkNoise1, 0, i2s1, 1);  // Right channel

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(4);

  // Configure pink noise
  pinkNoise1.amplitude(0.3);    // Volume

  // Start I2S output
  i2s1.begin();

  Serial.println("Pink Noise Generator (1/f noise)");
  Serial.println("More bass energy than white noise");
  Serial.println("Volume fades in and out");
}

void loop() {
  // Fade volume in and out smoothly
  static uint32_t next = millis() + 50;
  static float volume = 0.0;
  static float direction = 0.01;

  if (millis() >= next) {
    next = millis() + 50;

    volume += direction;

    if (volume >= 0.3) {
      direction = -0.01;
    } else if (volume <= 0.0) {
      direction = 0.01;
    }

    pinkNoise1.amplitude(volume);
  }
}
