// Amplifier/Attenuator Example
// Demonstrates AudioAmplifier
// Controls signal level with smooth ramping

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform    waveform1;     // Sound source
AudioAmplifier        amp1;          // Amplifier
AudioOutputI2S        i2s1;          // I2S output to PCM5102

AudioConnection       patchCord1(waveform1, 0, amp1, 0);
AudioConnection       patchCord2(amp1, 0, i2s1, 0);       // Left
AudioConnection       patchCord3(amp1, 0, i2s1, 1);       // Right

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(10);

  // Configure sound source
  waveform1.begin(0.8, 440, WAVEFORM_SAWTOOTH);

  // Set initial gain
  amp1.gain(0.5);  // 50% volume

  // Start I2S output
  i2s1.begin();

  Serial.println("Amplifier/Attenuator Example");
  Serial.println("Gain varies from 0.0 to 2.0");
  Serial.println("Values > 1.0 amplify, < 1.0 attenuate");
}

void loop() {
  // Sweep gain from 0.0 to 2.0
  static uint32_t next = millis() + 50;
  static float gain = 0.0;
  static float direction = 0.02;

  if (millis() >= next) {
    next = millis() + 50;

    gain += direction;

    if (gain >= 2.0) {
      direction = -0.02;
      Serial.println("\n>>> Decreasing gain <<<");
    } else if (gain <= 0.0) {
      direction = 0.02;
      Serial.println("\n>>> Increasing gain <<<");
    }

    amp1.gain(gain);

    // Print gain level every second
    static uint32_t printNext = millis() + 1000;
    if (millis() >= printNext) {
      printNext = millis() + 1000;

      Serial.print("Gain: ");
      Serial.print(gain, 2);

      if (gain < 1.0) {
        Serial.print(" (Attenuating - ");
        Serial.print(gain * 100, 0);
        Serial.println("%)");
      } else if (gain > 1.0) {
        Serial.print(" (Amplifying +");
        Serial.print((gain - 1.0) * 100, 0);
        Serial.println("%)");
      } else {
        Serial.println(" (Unity gain)");
      }
    }
  }
}
