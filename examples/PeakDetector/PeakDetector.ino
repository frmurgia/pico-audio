// Peak Detector Example
// Demonstrates AudioAnalyzePeak
// Detects peak signal level (useful for VU meters, limiters)

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      waveform1;      // Sound source
AudioAnalyzePeak        peak1;          // Peak detector
AudioOutputI2S          i2s1;           // I2S output to PCM5102

AudioConnection         patchCord1(waveform1, 0, peak1, 0);
AudioConnection         patchCord2(waveform1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(waveform1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(10);

  // Configure sound source
  waveform1.begin(0.3, 440, WAVEFORM_SINE);

  // Start I2S output
  i2s1.begin();

  Serial.println("Peak Detector Example");
  Serial.println("Measures peak signal amplitude");
  Serial.println("Volume varies, peak detector tracks it");
}

void loop() {
  // Read peak when available
  if (peak1.available()) {
    float peakValue = peak1.read();

    // Create simple ASCII VU meter
    Serial.print("Peak: ");
    Serial.print(peakValue, 3);
    Serial.print(" |");

    int bars = peakValue * 50;
    for (int i = 0; i < bars; i++) {
      Serial.print("=");
    }

    Serial.println("|");
  }

  // Vary amplitude to demonstrate peak tracking
  static uint32_t next = millis() + 50;
  static float amplitude = 0.1;
  static float direction = 0.02;

  if (millis() >= next) {
    next = millis() + 50;

    amplitude += direction;

    if (amplitude >= 0.8) {
      direction = -0.02;
    } else if (amplitude <= 0.1) {
      direction = 0.02;
    }

    waveform1.amplitude(amplitude);
  }
}
