// Mid-Side Processing Example
// Demonstrates AudioEffectMidSide
// Encodes/decodes mid-side stereo for advanced stereo processing

#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      left;           // Left channel source
AudioSynthWaveform      right;          // Right channel source
AudioEffectMidSide      midside1;       // Mid-side processor
AudioOutputI2S          i2s1;           // I2S output to PCM5102

AudioConnection         patchCord1(left, 0, midside1, 0);       // Left in
AudioConnection         patchCord2(right, 0, midside1, 1);      // Right in
AudioConnection         patchCord3(midside1, 0, i2s1, 0);       // Left out
AudioConnection         patchCord4(midside1, 1, i2s1, 1);       // Right out

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(15);

  // Configure left channel (lower frequency)
  left.begin(0.3, 330, WAVEFORM_SINE);

  // Configure right channel (higher frequency)
  right.begin(0.3, 440, WAVEFORM_SINE);

  // Configure mid-side
  midside1.encode();  // Encode to mid-side

  // Start I2S output
  i2s1.begin();

  Serial.println("Mid-Side Processing Example");
  Serial.println("Demonstrates stereo encoding/decoding");
}

void loop() {
  // Toggle between encode and decode modes
  static uint32_t next = millis() + 5000;
  static bool encoded = true;

  if (millis() >= next) {
    next = millis() + 5000;

    if (encoded) {
      midside1.decode();
      Serial.println("Mode: DECODE (mid-side back to L/R)");
    } else {
      midside1.encode();
      Serial.println("Mode: ENCODE (L/R to mid-side)");
    }

    encoded = !encoded;
  }
}
