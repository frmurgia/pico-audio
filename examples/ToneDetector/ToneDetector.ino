// Tone Detector Example
// Demonstrates AudioAnalyzeToneDetect
// Detects presence and amplitude of specific frequency

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform        waveform1;      // Sound source
AudioAnalyzeToneDetect    tone1;          // Tone detector
AudioOutputI2S            i2s1;           // I2S output to PCM5102

AudioConnection           patchCord1(waveform1, 0, tone1, 0);
AudioConnection           patchCord2(waveform1, 0, i2s1, 0);      // Left output
AudioConnection           patchCord3(waveform1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(15);

  // Configure sound source - sweep frequency
  waveform1.begin(0.5, 200, WAVEFORM_SINE);

  // Configure tone detector to look for 440 Hz (A4)
  tone1.frequency(440);
  tone1.threshold(0.1);  // Minimum threshold to detect

  // Start I2S output
  i2s1.begin();

  Serial.println("Tone Detector Example");
  Serial.println("Detecting 440 Hz (A4 note)");
  Serial.println("Sweeping frequency from 200 Hz to 800 Hz");
}

void loop() {
  // Read tone detector when available
  if (tone1.available()) {
    float confidence = tone1.read();

    if (confidence > 0.1) {
      Serial.print("440 Hz DETECTED! Confidence: ");
      Serial.print(confidence, 3);

      // Visual indicator
      Serial.print(" |");
      int bars = confidence * 50;
      for (int i = 0; i < bars; i++) {
        Serial.print("*");
      }
      Serial.println("|");
    }
  }

  // Sweep frequency
  static uint32_t next = millis() + 100;
  static float freq = 200.0;
  static float direction = 10.0;

  if (millis() >= next) {
    next = millis() + 100;

    freq += direction;

    if (freq >= 800.0) {
      direction = -10.0;
    } else if (freq <= 200.0) {
      direction = 10.0;
    }

    waveform1.frequency(freq);

    // Print current frequency occasionally
    static uint32_t printNext = millis() + 500;
    if (millis() >= printNext) {
      printNext = millis() + 500;
      Serial.print("Current frequency: ");
      Serial.print(freq, 1);
      Serial.println(" Hz");
    }
  }
}
