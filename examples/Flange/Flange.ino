// Flange Effect Example
// Demonstrates AudioEffectFlange
// Creates sweeping "jet plane" or "woosh" effect

#include <pico-audio.h>

// Create audio components
AudioSynthWaveform    waveform1;     // Sound source
AudioEffectFlange     flange1;       // Flange effect
AudioOutputI2S        i2s1;          // I2S output to PCM5102

// Allocate memory for flange delay line
int16_t flangeDelayLine[FLANGE_DELAY_LENGTH];

AudioConnection       patchCord1(waveform1, 0, flange1, 0);
AudioConnection       patchCord2(flange1, 0, i2s1, 0);      // Left output
AudioConnection       patchCord3(flange1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(50);

  // Configure sound source
  waveform1.begin(0.5, 440, WAVEFORM_SAWTOOTH);

  // Configure flange
  flange1.begin(flangeDelayLine, FLANGE_DELAY_LENGTH, 2, 3, 0.5);
  // Parameters: delayLine, length, offset, depth, delayRate

  // Start I2S output
  i2s1.begin();

  Serial.println("Flange Effect Example");
  Serial.println("Classic sweeping flange sound");
}

void loop() {
  // Change flange parameters over time
  static uint32_t next = millis() + 5000;
  static int depth = 3;

  if (millis() >= next) {
    next = millis() + 5000;

    depth += 2;
    if (depth > 10) depth = 1;

    flange1.begin(flangeDelayLine, FLANGE_DELAY_LENGTH, 2, depth, 0.5);

    Serial.print("Flange depth: ");
    Serial.println(depth);
  }
}
