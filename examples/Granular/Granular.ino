// Granular Synthesis Example
// Demonstrates AudioEffectGranular
// Creates granular effects by playing tiny overlapping pieces of audio

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      waveform1;      // Sound source
AudioEffectGranular     granular1;      // Granular effect
AudioOutputI2S          i2s1;           // I2S output to PCM5102

// Granular requires a delay buffer
#define GRANULAR_MEMORY_SIZE 12800  // 12800 samples = ~290ms at 44.1kHz
int16_t granularMemory[GRANULAR_MEMORY_SIZE];

AudioConnection         patchCord1(waveform1, 0, granular1, 0);
AudioConnection         patchCord2(granular1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(granular1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(50);

  // Configure sound source - slow sweep
  waveform1.begin(0.5, 110, WAVEFORM_SAWTOOTH);
  waveform1.frequencyModulation(2);  // Octave range

  // Configure granular effect
  granular1.begin(granularMemory, GRANULAR_MEMORY_SIZE);
  granular1.beginFreeze(100);     // Grain size in milliseconds
  granular1.setSpeed(1.0);        // Playback speed (1.0 = normal)

  // Start I2S output
  i2s1.begin();

  Serial.println("Granular Synthesis Example");
  Serial.println("Creates texture by repeating small grains");
}

void loop() {
  // Sweep frequency slowly
  static uint32_t next = millis() + 100;
  static float freq = 110.0;
  static float direction = 2.0;

  if (millis() >= next) {
    next = millis() + 100;

    freq += direction;
    if (freq >= 440.0) direction = -2.0;
    if (freq <= 110.0) direction = 2.0;

    waveform1.frequency(freq);
  }

  // Change granular parameters
  static uint32_t grainNext = millis() + 3000;
  static float grainSpeed = 1.0;

  if (millis() >= grainNext) {
    grainNext = millis() + 3000;

    grainSpeed += 0.25;
    if (grainSpeed > 2.0) grainSpeed = 0.5;

    granular1.setSpeed(grainSpeed);

    Serial.print("Grain playback speed: ");
    Serial.println(grainSpeed);
  }
}
