// Basic Waveforms Example
// Demonstrates AudioSynthWaveform
// Cycles through different waveform types: sine, sawtooth, square, triangle

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform    waveform1;     // Multi-waveform oscillator
AudioOutputI2S        i2s1;          // I2S output to PCM5102
AudioConnection       patchCord1(waveform1, 0, i2s1, 0);  // Left channel
AudioConnection       patchCord2(waveform1, 0, i2s1, 1);  // Right channel

const char* waveformNames[] = {
  "Sine",
  "Sawtooth",
  "Square",
  "Triangle",
  "Pulse"
};

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(8);

  // Configure waveform
  waveform1.begin(0.3, 220, WAVEFORM_SINE);  // Volume, frequency, type

  // Start I2S output
  i2s1.begin();

  Serial.println("Basic Waveforms Demo");
  Serial.println("Cycles through different waveform types every 3 seconds");
}

void loop() {
  // Cycle through waveforms every 3 seconds
  static uint32_t next = millis() + 3000;
  static int waveType = WAVEFORM_SINE;

  if (millis() >= next) {
    next = millis() + 3000;

    // Cycle through waveform types
    switch (waveType) {
      case WAVEFORM_SINE:
        waveType = WAVEFORM_SAWTOOTH;
        waveform1.begin(WAVEFORM_SAWTOOTH);
        Serial.println("Waveform: SAWTOOTH");
        break;
      case WAVEFORM_SAWTOOTH:
        waveType = WAVEFORM_SQUARE;
        waveform1.begin(WAVEFORM_SQUARE);
        Serial.println("Waveform: SQUARE");
        break;
      case WAVEFORM_SQUARE:
        waveType = WAVEFORM_TRIANGLE;
        waveform1.begin(WAVEFORM_TRIANGLE);
        Serial.println("Waveform: TRIANGLE");
        break;
      case WAVEFORM_TRIANGLE:
        waveType = WAVEFORM_PULSE;
        waveform1.begin(WAVEFORM_PULSE);
        waveform1.pulseWidth(0.5);  // 50% duty cycle
        Serial.println("Waveform: PULSE");
        break;
      case WAVEFORM_PULSE:
        waveType = WAVEFORM_SINE;
        waveform1.begin(WAVEFORM_SINE);
        Serial.println("Waveform: SINE");
        break;
    }
  }
}
