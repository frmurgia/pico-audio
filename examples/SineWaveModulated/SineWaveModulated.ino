// Frequency Modulated Sine Wave Example
// Demonstrates AudioSynthWaveformSineModulated
// Uses one sine wave to modulate the frequency of another (FM synthesis)

#include <pico-audio.h>

// Create audio components
AudioSynthWaveformSine          lfo;           // Low frequency oscillator
AudioSynthWaveformSineModulated carrier;       // FM carrier oscillator
AudioOutputI2S                  i2s1;          // I2S output to PCM5102
AudioConnection                 patchCord1(lfo, 0, carrier, 0);       // LFO modulates carrier
AudioConnection                 patchCord2(carrier, 0, i2s1, 0);      // Left channel
AudioConnection                 patchCord3(carrier, 0, i2s1, 1);      // Right channel

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(8);

  // Configure LFO (modulator)
  lfo.amplitude(1.0);           // Maximum modulation
  lfo.frequency(5.0);           // 5 Hz vibrato

  // Configure carrier
  carrier.amplitude(0.4);       // Volume
  carrier.frequency(440);       // Base frequency (A4)

  // Start I2S output
  i2s1.begin();

  Serial.println("Playing frequency modulated sine wave");
  Serial.println("LFO creates vibrato effect");
}

void loop() {
  // Change LFO rate every 3 seconds
  static uint32_t next = millis() + 3000;
  static float lfoFreq = 5.0;

  if (millis() >= next) {
    next = millis() + 3000;

    lfoFreq += 2.0;
    if (lfoFreq > 20.0) lfoFreq = 1.0;

    lfo.frequency(lfoFreq);

    Serial.print("LFO frequency: ");
    Serial.print(lfoFreq);
    Serial.println(" Hz");
  }
}
