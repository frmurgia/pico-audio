// Biquad Filter Example
// Demonstrates AudioFilterBiquad
// Versatile 2nd-order IIR filter: lowpass, highpass, bandpass, notch

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthNoiseWhite    noise1;        // White noise source for testing
AudioFilterBiquad       biquad1;       // Biquad filter
AudioOutputI2S          i2s1;          // I2S output to PCM5102

AudioConnection         patchCord1(noise1, 0, biquad1, 0);
AudioConnection         patchCord2(biquad1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(biquad1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(10);

  // Configure noise source
  noise1.amplitude(0.4);

  // Configure biquad filter - start with lowpass at 1000 Hz
  biquad1.setLowpass(0, 1000, 0.707);  // stage, frequency, Q

  // Start I2S output
  i2s1.begin();

  Serial.println("Biquad Filter Example");
  Serial.println("Filters white noise through different filter types");
  Serial.println("Starting with lowpass at 1000 Hz");
}

void loop() {
  // Cycle through different filter types every 4 seconds
  static uint32_t next = millis() + 4000;
  static int filterType = 0;

  if (millis() >= next) {
    next = millis() + 4000;

    switch (filterType) {
      case 0:
        biquad1.setHighpass(0, 1000, 0.707);
        Serial.println("Filter: HIGHPASS at 1000 Hz");
        break;
      case 1:
        biquad1.setBandpass(0, 1000, 2.0);
        Serial.println("Filter: BANDPASS at 1000 Hz");
        break;
      case 2:
        biquad1.setNotch(0, 1000, 2.0);
        Serial.println("Filter: NOTCH at 1000 Hz");
        break;
      case 3:
        biquad1.setLowpass(0, 500, 0.707);
        Serial.println("Filter: LOWPASS at 500 Hz");
        break;
      case 4:
        biquad1.setLowpass(0, 2000, 0.707);
        Serial.println("Filter: LOWPASS at 2000 Hz");
        break;
    }

    filterType = (filterType + 1) % 5;
  }
}
