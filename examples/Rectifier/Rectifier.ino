// Rectifier Effect Example
// Demonstrates AudioEffectRectifier
// Full-wave and half-wave rectification for octave-up effects

#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      waveform1;      // Sound source
AudioEffectRectifier    rectifier1;     // Rectifier effect
AudioOutputI2S          i2s1;           // I2S output to PCM5102

AudioConnection         patchCord1(waveform1, 0, rectifier1, 0);
AudioConnection         patchCord2(rectifier1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(rectifier1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(8);

  // Configure sound source
  waveform1.begin(0.5, 110, WAVEFORM_SINE);  // Low A note

  // Configure rectifier (full wave creates octave up)
  rectifier1.fullWave();

  // Start I2S output
  i2s1.begin();

  Serial.println("Rectifier Effect Example");
  Serial.println("Full-wave rectification = octave up");
  Serial.println("Half-wave rectification = different harmonics");
}

void loop() {
  // Toggle between full-wave and half-wave
  static uint32_t next = millis() + 4000;
  static bool fullWave = true;

  if (millis() >= next) {
    next = millis() + 4000;

    if (fullWave) {
      rectifier1.halfWave();
      Serial.println("Mode: HALF-WAVE rectification");
      fullWave = false;
    } else {
      rectifier1.fullWave();
      Serial.println("Mode: FULL-WAVE rectification (octave up)");
      fullWave = true;
    }
  }
}
