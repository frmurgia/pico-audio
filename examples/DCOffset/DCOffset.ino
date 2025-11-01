// DC Offset Example
// Demonstrates AudioSynthWaveformDc
// Generates constant DC level (useful for control signals and testing)

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveformDc  dc1;         // DC generator
AudioSynthWaveform    osc1;        // Oscillator to modulate
AudioOutputI2S        i2s1;        // I2S output to PCM5102
AudioConnection       patchCord1(dc1, 0, osc1, 0);    // DC modulates oscillator
AudioConnection       patchCord2(osc1, 0, i2s1, 0);   // Left channel
AudioConnection       patchCord3(osc1, 0, i2s1, 1);   // Right channel

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(8);

  // Configure oscillator
  osc1.begin(0.4, 440, WAVEFORM_SINE);

  // Configure DC offset (starts at 0)
  dc1.amplitude(0.0);

  // Start I2S output
  i2s1.begin();

  Serial.println("DC Offset / Control Voltage Example");
  Serial.println("DC level varies from -1.0 to +1.0");
  Serial.println("Listen to how it affects the oscillator");
}

void loop() {
  // Sweep DC level continuously
  static uint32_t next = millis() + 100;
  static float dcLevel = -1.0;
  static float direction = 0.05;

  if (millis() >= next) {
    next = millis() + 100;

    dcLevel += direction;

    if (dcLevel >= 1.0) {
      direction = -0.05;
    } else if (dcLevel <= -1.0) {
      direction = 0.05;
    }

    dc1.amplitude(dcLevel);

    // Print DC level every second
    static uint32_t printNext = millis() + 1000;
    if (millis() >= printNext) {
      printNext = millis() + 1000;
      Serial.print("DC Level: ");
      Serial.println(dcLevel);
    }
  }
}
