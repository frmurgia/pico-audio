// Simple Sine Wave Example
// Demonstrates AudioSynthWaveformSine
// Plays a pure sine wave at 440 Hz (A note)

#include <pico-audio.h>

// Create audio components
AudioSynthWaveformSine   sine1;       // Sine wave oscillator
AudioOutputI2S           i2s1;        // I2S output to PCM5102
AudioConnection          patchCord1(sine1, 0, i2s1, 0);  // Left channel
AudioConnection          patchCord2(sine1, 0, i2s1, 1);  // Right channel

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(4);

  // Configure sine wave
  sine1.amplitude(0.5);      // Volume (0.0 to 1.0)
  sine1.frequency(440);      // A4 note (440 Hz)

  // Start I2S output
  i2s1.begin();

  Serial.println("Playing 440 Hz sine wave");
}

void loop() {
  // Print CPU and memory usage every 2 seconds
  static uint32_t next = millis() + 2000;

  if (millis() >= next) {
    next = millis() + 2000;

    Serial.print("CPU: ");
    Serial.print(AudioProcessorUsageMax());
    Serial.print("% Memory: ");
    Serial.println(AudioMemoryUsageMax());

    AudioProcessorUsageMaxReset();
  }
}
