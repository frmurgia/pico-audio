// RMS Analyzer Example
// Demonstrates AudioAnalyzeRMS
// Measures Root Mean Square (average power/loudness)

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      waveform1;      // Sound source
AudioAnalyzeRMS         rms1;           // RMS analyzer
AudioOutputI2S          i2s1;           // I2S output to PCM5102

AudioConnection         patchCord1(waveform1, 0, rms1, 0);
AudioConnection         patchCord2(waveform1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(waveform1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(10);

  // Configure sound source
  waveform1.begin(0.5, 440, WAVEFORM_SINE);

  // Start I2S output
  i2s1.begin();

  Serial.println("RMS (Root Mean Square) Analyzer");
  Serial.println("Measures average signal power/loudness");
  Serial.println("Compares different waveform types");
}

void loop() {
  // Read RMS when available
  if (rms1.available()) {
    float rmsValue = rms1.read();

    // Display RMS value and meter
    Serial.print("RMS: ");
    Serial.print(rmsValue, 3);
    Serial.print(" |");

    int bars = rmsValue * 50;
    for (int i = 0; i < bars; i++) {
      Serial.print("#");
    }

    Serial.println("|");
  }

  // Cycle through different waveforms to show RMS differences
  static uint32_t next = millis() + 5000;
  static int waveType = WAVEFORM_SINE;

  if (millis() >= next) {
    next = millis() + 5000;

    switch (waveType) {
      case WAVEFORM_SINE:
        waveType = WAVEFORM_SQUARE;
        waveform1.begin(0.5, 440, WAVEFORM_SQUARE);
        Serial.println("\n>>> SQUARE wave - higher RMS");
        break;
      case WAVEFORM_SQUARE:
        waveType = WAVEFORM_TRIANGLE;
        waveform1.begin(0.5, 440, WAVEFORM_TRIANGLE);
        Serial.println("\n>>> TRIANGLE wave");
        break;
      case WAVEFORM_TRIANGLE:
        waveType = WAVEFORM_SAWTOOTH;
        waveform1.begin(0.5, 440, WAVEFORM_SAWTOOTH);
        Serial.println("\n>>> SAWTOOTH wave");
        break;
      case WAVEFORM_SAWTOOTH:
        waveType = WAVEFORM_SINE;
        waveform1.begin(0.5, 440, WAVEFORM_SINE);
        Serial.println("\n>>> SINE wave - lower RMS");
        break;
    }
  }
}
