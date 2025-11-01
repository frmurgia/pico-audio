// Chorus Effect Example
// Demonstrates AudioEffectChorus
// Creates thick, detuned sound by mixing delayed copies

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform    waveform1;     // Sound source
AudioEffectChorus     chorus1;       // Chorus effect
AudioOutputI2S        i2s1;          // I2S output to PCM5102

AudioConnection       patchCord1(waveform1, 0, chorus1, 0);
AudioConnection       patchCord2(chorus1, 0, i2s1, 0);      // Left output
AudioConnection       patchCord3(chorus1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(50);

  // Configure sound source
  waveform1.begin(0.5, 220, WAVEFORM_SAWTOOTH);  // A3 note

  // Configure chorus
  // voices: number of chorus voices (1-4)
  chorus1.voices(3);

  // Start I2S output
  i2s1.begin();

  Serial.println("Chorus Effect Example");
  Serial.println("Creates rich, detuned sound");
  Serial.println("Playing sustained note with chorus");
}

void loop() {
  // Vary number of voices
  static uint32_t next = millis() + 3000;
  static int numVoices = 1;

  if (millis() >= next) {
    next = millis() + 3000;

    numVoices++;
    if (numVoices > 4) numVoices = 1;

    chorus1.voices(numVoices);

    Serial.print("Chorus voices: ");
    Serial.println(numVoices);
  }
}
