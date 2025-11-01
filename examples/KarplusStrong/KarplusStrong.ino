// Karplus-Strong String Synthesis Example
// Demonstrates AudioSynthKarplusStrong
// Physical modeling synthesis for plucked string sounds

#include <pico-audio.h>

// Create audio components
AudioSynthKarplusStrong  string1;     // String synth
AudioOutputI2S           i2s1;        // I2S output to PCM5102
AudioConnection          patchCord1(string1, 0, i2s1, 0);  // Left channel
AudioConnection          patchCord2(string1, 0, i2s1, 1);  // Right channel

// Musical notes (MIDI note numbers)
const uint8_t melody[] = {60, 62, 64, 65, 67, 69, 71, 72};  // C major scale
const int melodyLength = 8;

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(10);

  // Start I2S output
  i2s1.begin();

  Serial.println("Karplus-Strong String Synthesis");
  Serial.println("Physical modeling of plucked strings");
  Serial.println("Playing C major scale");
}

void loop() {
  // Play notes from the melody
  static uint32_t next = millis() + 500;
  static int noteIndex = 0;

  if (millis() >= next) {
    next = millis() + 500;

    // Get MIDI note number
    uint8_t midiNote = melody[noteIndex];

    // Calculate frequency from MIDI note
    float freq = 440.0 * pow(2.0, (midiNote - 69) / 12.0);

    // Pluck the string
    string1.noteOn(freq, 0.8);  // frequency, velocity

    Serial.print("Note: ");
    Serial.print(midiNote);
    Serial.print(" Frequency: ");
    Serial.print(freq);
    Serial.println(" Hz");

    // Move to next note
    noteIndex = (noteIndex + 1) % melodyLength;
  }
}
