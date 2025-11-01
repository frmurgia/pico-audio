// Ring Modulation Example
// Demonstrates AudioEffectMultiply
// Multiplies two audio signals for ring modulation effects

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      carrier;        // Carrier oscillator
AudioSynthWaveform      modulator;      // Modulator oscillator
AudioEffectMultiply     multiply1;      // Ring modulator
AudioOutputI2S          i2s1;           // I2S output to PCM5102

AudioConnection         patchCord1(carrier, 0, multiply1, 0);
AudioConnection         patchCord2(modulator, 0, multiply1, 1);
AudioConnection         patchCord3(multiply1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord4(multiply1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(10);

  // Configure carrier (main signal)
  carrier.begin(0.8, 440, WAVEFORM_SINE);

  // Configure modulator
  modulator.begin(1.0, 100, WAVEFORM_SINE);  // Start with low frequency

  // Start I2S output
  i2s1.begin();

  Serial.println("Ring Modulation Example");
  Serial.println("Carrier: 440 Hz, Modulator: sweeping");
  Serial.println("Creates sum and difference frequencies");
}

void loop() {
  // Sweep modulator frequency
  static uint32_t next = millis() + 100;
  static float modFreq = 1.0;
  static float direction = 5.0;

  if (millis() >= next) {
    next = millis() + 100;

    modFreq += direction;

    if (modFreq >= 500.0) {
      direction = -5.0;
    } else if (modFreq <= 1.0) {
      direction = 5.0;
    }

    modulator.frequency(modFreq);

    // Print every second
    static uint32_t printNext = millis() + 1000;
    if (millis() >= printNext) {
      printNext = millis() + 1000;
      Serial.print("Modulator frequency: ");
      Serial.print(modFreq);
      Serial.println(" Hz");
    }
  }
}
