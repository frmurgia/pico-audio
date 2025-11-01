// High Resolution Sine Wave Example
// Demonstrates AudioSynthWaveformSineHires
// Uses higher precision for lower distortion

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveformSineHires   sineHires1;   // High-res sine oscillator
AudioOutputI2S                i2s1;          // I2S output to PCM5102
AudioConnection               patchCord1(sineHires1, 0, i2s1, 0);  // Left
AudioConnection               patchCord2(sineHires1, 0, i2s1, 1);  // Right

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(4);

  // Configure high-res sine wave
  sineHires1.amplitude(0.3);      // Volume (0.0 to 1.0)
  sineHires1.frequency(1000);     // 1 kHz tone

  // Start I2S output
  i2s1.begin();

  Serial.println("Playing 1 kHz high-resolution sine wave");
  Serial.println("Lower distortion compared to standard sine");
}

void loop() {
  // Sweep frequency slowly from 200 Hz to 2000 Hz
  static uint32_t next = millis() + 50;
  static float freq = 200.0;
  static float direction = 1.0;

  if (millis() >= next) {
    next = millis() + 50;

    freq += direction * 10.0;

    if (freq >= 2000.0) {
      direction = -1.0;
    } else if (freq <= 200.0) {
      direction = 1.0;
    }

    sineHires1.frequency(freq);
  }
}
