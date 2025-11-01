// Freeverb Example
// Demonstrates AudioEffectFreeverb
// High-quality reverb with room size and damping controls

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      waveform1;     // Sound source
AudioEffectFreeverb     freeverb1;     // Freeverb effect
AudioMixer4             mixer1;        // Mix dry and wet signals
AudioOutputI2S          i2s1;          // I2S output to PCM5102

AudioConnection         patchCord1(waveform1, 0, mixer1, 0);       // Dry signal
AudioConnection         patchCord2(waveform1, 0, freeverb1, 0);    // To reverb
AudioConnection         patchCord3(freeverb1, 0, mixer1, 1);       // Reverb signal
AudioConnection         patchCord4(mixer1, 0, i2s1, 0);            // Left output
AudioConnection         patchCord5(mixer1, 0, i2s1, 1);            // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(120);

  // Configure sound source
  waveform1.begin(0.4, 523, WAVEFORM_TRIANGLE);  // C5 note

  // Configure Freeverb
  freeverb1.roomsize(0.8);   // Room size (0.0 to 1.0)
  freeverb1.damping(0.5);    // High frequency damping (0.0 to 1.0)

  // Mix 60% dry, 40% wet
  mixer1.gain(0, 0.6);  // Dry signal
  mixer1.gain(1, 0.4);  // Reverb signal

  // Start I2S output
  i2s1.begin();

  Serial.println("Freeverb High-Quality Reverb");
  Serial.println("Room size: 0.8, Damping: 0.5");
}

void loop() {
  // Play arpeggio pattern
  static uint32_t next = millis() + 400;
  static int noteIndex = 0;
  static const float notes[] = {523.25, 659.25, 783.99, 1046.50};  // C, E, G, C

  if (millis() >= next) {
    next = millis() + 400;

    waveform1.frequency(notes[noteIndex]);

    Serial.print("Playing note: ");
    Serial.print(notes[noteIndex]);
    Serial.println(" Hz");

    noteIndex = (noteIndex + 1) % 4;
  }
}
