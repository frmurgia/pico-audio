// Note Frequency Detector Example
// Demonstrates AudioAnalyzeNoteFrequency
// Detects musical note frequencies (guitar tuner, pitch tracker)

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform            waveform1;      // Sound source
AudioAnalyzeNoteFrequency     notefreq1;      // Note frequency detector
AudioOutputI2S                i2s1;           // I2S output to PCM5102

AudioConnection               patchCord1(waveform1, 0, notefreq1, 0);
AudioConnection               patchCord2(waveform1, 0, i2s1, 0);      // Left output
AudioConnection               patchCord3(waveform1, 0, i2s1, 1);      // Right output

// Note names
const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(30);

  // Configure sound source
  waveform1.begin(0.5, 440, WAVEFORM_SINE);  // Start with A4

  // Configure note frequency detector
  notefreq1.begin(0.15);  // Threshold (0.0 to 1.0)

  // Start I2S output
  i2s1.begin();

  Serial.println("Note Frequency Detector");
  Serial.println("Musical pitch detection / Guitar tuner");
  Serial.println("Playing different notes");

  delay(1000);
}

void loop() {
  // Read note frequency when available
  if (notefreq1.available()) {
    float frequency = notefreq1.read();
    float probability = notefreq1.probability();

    if (probability > 0.9) {  // Good confidence
      // Convert frequency to MIDI note number
      float midiNote = 12.0 * log2(frequency / 440.0) + 69;
      int noteNum = round(midiNote);
      int cents = round((midiNote - noteNum) * 100);

      // Get note name
      const char* noteName = noteNames[noteNum % 12];
      int octave = (noteNum / 12) - 1;

      Serial.print("Detected: ");
      Serial.print(frequency, 2);
      Serial.print(" Hz = ");
      Serial.print(noteName);
      Serial.print(octave);

      if (cents != 0) {
        Serial.print(" ");
        Serial.print(cents > 0 ? "+" : "");
        Serial.print(cents);
        Serial.print(" cents");
      }

      Serial.print(" (confidence: ");
      Serial.print(probability, 2);
      Serial.println(")");
    }
  }

  // Play different notes
  static uint32_t next = millis() + 3000;
  static const float notes[] = {261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25};
  static int noteIndex = 0;

  if (millis() >= next) {
    next = millis() + 3000;

    waveform1.frequency(notes[noteIndex]);

    Serial.print("\n>>> Playing ");
    Serial.print(notes[noteIndex]);
    Serial.println(" Hz\n");

    noteIndex = (noteIndex + 1) % 8;
  }
}
