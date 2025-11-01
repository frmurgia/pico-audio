// PWM Synthesis Example
// Demonstrates AudioSynthWaveformPWM
// Creates pulse width modulation waveform with variable duty cycle

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveformPWM   pwm1;        // PWM oscillator
AudioOutputI2S          i2s1;        // I2S output to PCM5102
AudioConnection         patchCord1(pwm1, 0, i2s1, 0);  // Left channel
AudioConnection         patchCord2(pwm1, 0, i2s1, 1);  // Right channel

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(8);

  // Configure PWM waveform
  pwm1.amplitude(0.4);          // Volume
  pwm1.frequency(110);          // Low A note
  pwm1.pulseWidth(0.5);         // 50% duty cycle

  // Start I2S output
  i2s1.begin();

  Serial.println("PWM Synthesis Demo");
  Serial.println("Duty cycle sweeps from 10% to 90%");
}

void loop() {
  // Sweep pulse width continuously
  static uint32_t next = millis() + 20;
  static float pulseWidth = 0.1;
  static float direction = 0.01;

  if (millis() >= next) {
    next = millis() + 20;

    pulseWidth += direction;

    if (pulseWidth >= 0.9) {
      direction = -0.01;
    } else if (pulseWidth <= 0.1) {
      direction = 0.01;
    }

    pwm1.pulseWidth(pulseWidth);

    // Print pulse width every second
    static uint32_t printNext = millis() + 1000;
    if (millis() >= printNext) {
      printNext = millis() + 1000;
      Serial.print("Pulse Width: ");
      Serial.print(pulseWidth * 100);
      Serial.println("%");
    }
  }
}
