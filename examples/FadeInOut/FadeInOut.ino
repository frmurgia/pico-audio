// Fade In/Out Example
// Demonstrates AudioEffectFade
// Smooth volume fades to prevent clicks and pops

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      waveform1;      // Sound source
AudioEffectFade         fade1;          // Fade effect
AudioOutputI2S          i2s1;           // I2S output to PCM5102

AudioConnection         patchCord1(waveform1, 0, fade1, 0);
AudioConnection         patchCord2(fade1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(fade1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(8);

  // Configure sound source
  waveform1.begin(0.8, 440, WAVEFORM_SQUARE);

  // Start with fade at 0
  fade1.fadeOut(0);

  // Start I2S output
  i2s1.begin();

  Serial.println("Fade In/Out Example");
  Serial.println("Smooth volume transitions");
}

void loop() {
  // Alternate between fade in and fade out
  static uint32_t next = millis() + 3000;
  static bool fadedIn = false;

  if (millis() >= next) {
    next = millis() + 3000;

    if (fadedIn) {
      // Fade out over 2 seconds
      fade1.fadeOut(2000);
      Serial.println("Fading OUT over 2 seconds");
      fadedIn = false;
    } else {
      // Fade in over 2 seconds
      fade1.fadeIn(2000);
      Serial.println("Fading IN over 2 seconds");
      fadedIn = true;
    }
  }
}
