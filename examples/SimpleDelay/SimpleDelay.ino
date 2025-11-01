// Simple Delay/Echo Example
// Demonstrates AudioEffectDelay
// Creates echo effect with adjustable delay time

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform    waveform1;     // Sound source
AudioEffectDelay      delay1;        // Delay effect
AudioMixer4           mixer1;        // Mix dry and wet signals
AudioOutputI2S        i2s1;          // I2S output to PCM5102

AudioConnection       patchCord1(waveform1, 0, mixer1, 0);     // Dry signal
AudioConnection       patchCord2(waveform1, 0, delay1, 0);     // To delay
AudioConnection       patchCord3(delay1, 0, mixer1, 1);        // Delayed signal
AudioConnection       patchCord4(mixer1, 0, i2s1, 0);          // Left output
AudioConnection       patchCord5(mixer1, 0, i2s1, 1);          // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory (delay needs more memory!)
  AudioMemory(200);

  // Configure sound source
  waveform1.begin(0.3, 330, WAVEFORM_SAWTOOTH);

  // Configure delay (200ms delay)
  delay1.delay(0, 200);  // channel 0, 200 milliseconds

  // Mix 50% dry, 50% wet
  mixer1.gain(0, 0.5);  // Dry signal
  mixer1.gain(1, 0.5);  // Delayed signal

  // Start I2S output
  i2s1.begin();

  Serial.println("Simple Delay Effect");
  Serial.println("200ms echo on sawtooth wave");
}

void loop() {
  // Play notes with delay effect
  static uint32_t next = millis() + 1000;
  static bool noteOn = false;

  if (millis() >= next) {
    next = millis() + 1000;

    if (noteOn) {
      waveform1.amplitude(0.0);
      noteOn = false;
    } else {
      waveform1.amplitude(0.3);
      noteOn = true;
      Serial.println("Note triggered - listen for echo!");
    }
  }
}
