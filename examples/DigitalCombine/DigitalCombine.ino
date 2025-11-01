// Digital Combine Example
// Demonstrates AudioEffectDigitalCombine
// Bitwise operations (AND, OR, XOR) on audio signals

#include <pico-audio.h>

// Create audio components
AudioSynthWaveform          waveform1;      // First signal
AudioSynthWaveform          waveform2;      // Second signal
AudioEffectDigitalCombine   combine1;       // Digital combine effect
AudioOutputI2S              i2s1;           // I2S output to PCM5102

AudioConnection             patchCord1(waveform1, 0, combine1, 0);
AudioConnection             patchCord2(waveform2, 0, combine1, 1);
AudioConnection             patchCord3(combine1, 0, i2s1, 0);      // Left output
AudioConnection             patchCord4(combine1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(10);

  // Configure first oscillator
  waveform1.begin(0.5, 220, WAVEFORM_SQUARE);

  // Configure second oscillator
  waveform2.begin(0.5, 330, WAVEFORM_SQUARE);

  // Start with OR operation
  combine1.setCombineMode(AudioEffectDigitalCombine::OR);

  // Start I2S output
  i2s1.begin();

  Serial.println("Digital Combine Example");
  Serial.println("Bitwise operations on audio: AND, OR, XOR");
  Serial.println("Two square waves combined digitally");
}

void loop() {
  // Cycle through different combine modes
  static uint32_t next = millis() + 3000;
  static int mode = 0;

  if (millis() >= next) {
    next = millis() + 3000;

    switch (mode) {
      case 0:
        combine1.setCombineMode(AudioEffectDigitalCombine::AND);
        Serial.println("Combine mode: AND (only where both signals are high)");
        break;
      case 1:
        combine1.setCombineMode(AudioEffectDigitalCombine::OR);
        Serial.println("Combine mode: OR (where either signal is high)");
        break;
      case 2:
        combine1.setCombineMode(AudioEffectDigitalCombine::XOR);
        Serial.println("Combine mode: XOR (exclusive or - different bits)");
        break;
    }

    mode = (mode + 1) % 3;
  }
}
