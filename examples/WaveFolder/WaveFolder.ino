// Wave Folder Effect Example
// Demonstrates AudioEffectWaveFolder
// Folds waveform back on itself for complex harmonic distortion

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      waveform1;      // Sound source
AudioEffectWaveFolder   wavefolder1;    // Wavefolder effect
AudioOutputI2S          i2s1;           // I2S output to PCM5102

AudioConnection         patchCord1(waveform1, 0, wavefolder1, 0);
AudioConnection         patchCord2(wavefolder1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(wavefolder1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(10);

  // Configure sound source
  waveform1.begin(0.5, 220, WAVEFORM_TRIANGLE);

  // Start I2S output
  i2s1.begin();

  Serial.println("Wave Folder Effect Example");
  Serial.println("Creates complex harmonics by folding waveform");
  Serial.println("Increasing amplitude to increase folding");
}

void loop() {
  // Sweep amplitude to increase wave folding
  static uint32_t next = millis() + 50;
  static float amplitude = 0.1;
  static float direction = 0.01;

  if (millis() >= next) {
    next = millis() + 50;

    amplitude += direction;

    if (amplitude >= 1.2) {
      direction = -0.01;
    } else if (amplitude <= 0.1) {
      direction = 0.01;
    }

    waveform1.amplitude(amplitude);

    // Print every second
    static uint32_t printNext = millis() + 1000;
    if (millis() >= printNext) {
      printNext = millis() + 1000;
      Serial.print("Amplitude (fold amount): ");
      Serial.println(amplitude);
    }
  }
}
