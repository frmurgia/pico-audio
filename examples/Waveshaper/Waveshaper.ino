// Waveshaper Distortion Example
// Demonstrates AudioEffectWaveshaper
// Non-linear waveshaping for distortion and harmonics

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      waveform1;      // Sound source
AudioEffectWaveshaper   waveshaper1;    // Waveshaper effect
AudioOutputI2S          i2s1;           // I2S output to PCM5102

AudioConnection         patchCord1(waveform1, 0, waveshaper1, 0);
AudioConnection         patchCord2(waveshaper1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(waveshaper1, 0, i2s1, 1);      // Right output

// Waveshaper transfer function (simple soft clipping curve)
float shaperFunction(float input) {
  // Soft saturation curve
  if (input > 1.0) return 1.0;
  if (input < -1.0) return -1.0;
  return input * (1.5 - 0.5 * input * input);
}

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(10);

  // Configure sound source
  waveform1.begin(0.8, 110, WAVEFORM_SINE);  // Drive it hard for distortion

  // Configure waveshaper
  waveshaper1.shape(shaperFunction, -1.0, 1.0, 1024);
  // Parameters: function, min input, max input, number of points

  // Start I2S output
  i2s1.begin();

  Serial.println("Waveshaper Distortion Example");
  Serial.println("Soft saturation adds harmonics to sine wave");
}

void loop() {
  // Vary input level to change distortion amount
  static uint32_t next = millis() + 100;
  static float amplitude = 0.1;
  static float direction = 0.02;

  if (millis() >= next) {
    next = millis() + 100;

    amplitude += direction;

    if (amplitude >= 1.0) {
      direction = -0.02;
    } else if (amplitude <= 0.1) {
      direction = 0.02;
    }

    waveform1.amplitude(amplitude);

    // Print every second
    static uint32_t printNext = millis() + 1000;
    if (millis() >= printNext) {
      printNext = millis() + 1000;
      Serial.print("Drive level: ");
      Serial.print(amplitude * 100);
      Serial.println("%");
    }
  }
}
