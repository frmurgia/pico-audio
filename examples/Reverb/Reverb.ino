// Simple Reverb Example
// Demonstrates AudioEffectReverb
// Adds reverb/room ambience to sound

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform    waveform1;     // Sound source
AudioEffectReverb     reverb1;       // Reverb effect
AudioMixer4           mixer1;        // Mix dry and wet signals
AudioOutputI2S        i2s1;          // I2S output to PCM5102

AudioConnection       patchCord1(waveform1, 0, mixer1, 0);     // Dry signal
AudioConnection       patchCord2(waveform1, 0, reverb1, 0);    // To reverb
AudioConnection       patchCord3(reverb1, 0, mixer1, 1);       // Reverb signal
AudioConnection       patchCord4(mixer1, 0, i2s1, 0);          // Left output
AudioConnection       patchCord5(mixer1, 0, i2s1, 1);          // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(120);

  // Configure sound source
  waveform1.begin(0.5, 440, WAVEFORM_SINE);

  // Configure reverb
  reverb1.reverbTime(0.5);  // Reverb time (0.0 to 1.0)

  // Mix 70% dry, 30% wet
  mixer1.gain(0, 0.7);  // Dry signal
  mixer1.gain(1, 0.3);  // Reverb signal

  // Start I2S output
  i2s1.begin();

  Serial.println("Reverb Effect Example");
  Serial.println("Playing notes with room ambience");
}

void loop() {
  // Play short notes to hear reverb tail
  static uint32_t next = millis() + 1500;
  static bool noteOn = false;

  if (millis() >= next) {
    next = millis() + 1500;

    if (noteOn) {
      waveform1.amplitude(0.0);
      noteOn = false;
      Serial.println("Note OFF - hear reverb tail");
    } else {
      waveform1.amplitude(0.5);
      noteOn = true;
      Serial.println("Note ON");
    }
  }
}
