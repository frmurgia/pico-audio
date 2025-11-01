// FFT Spectrum Analyzer Example
// Demonstrates AudioAnalyzeFFT1024
// Performs 1024-point FFT for frequency analysis
// Note: FFT256 has issues on RP2350, use FFT1024 instead

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      waveform1;      // Sound source
AudioAnalyzeFFT1024     fft1024;        // FFT analyzer
AudioOutputI2S          i2s1;           // I2S output to PCM5102

AudioConnection         patchCord1(waveform1, 0, fft1024, 0);
AudioConnection         patchCord2(waveform1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(waveform1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(30);

  // Configure sound source
  waveform1.begin(0.5, 440, WAVEFORM_SINE);  // A4 note

  // FFT window type
  fft1024.windowFunction(AudioWindowHanning1024);

  // Start I2S output
  i2s1.begin();

  Serial.println("FFT 1024-Point Spectrum Analyzer");
  Serial.println("Analyzing frequency content");

  delay(1000);
}

void loop() {
  // Read FFT when available
  if (fft1024.available()) {
    // Print the strongest frequency bins
    Serial.println("\n=== FFT Analysis ===");

    // Print first 40 bins (covers 0-3.5 kHz approximately)
    for (int i = 0; i < 40; i++) {
      float magnitude = fft1024.read(i);

      if (magnitude > 0.01) {  // Only print significant bins
        float frequency = i * 43.066;  // 44100 Hz / 1024 bins

        Serial.print("Bin ");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(frequency, 1);
        Serial.print(" Hz = ");
        Serial.println(magnitude, 4);
      }
    }
  }

  // Change frequency every 5 seconds
  static uint32_t next = millis() + 5000;
  static float freq = 440.0;

  if (millis() >= next) {
    next = millis() + 5000;

    freq *= 1.5;
    if (freq > 2000.0) freq = 220.0;

    waveform1.frequency(freq);

    Serial.print("\n>>> Changing to ");
    Serial.print(freq);
    Serial.println(" Hz\n");
  }
}
