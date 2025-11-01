// State Variable Filter Example
// Demonstrates AudioFilterStateVariable
// Universal filter with simultaneous lowpass, bandpass, and highpass outputs

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform            waveform1;    // Sound source
AudioFilterStateVariable      svf1;         // State variable filter
AudioOutputI2S                i2s1;         // I2S output to PCM5102

AudioConnection               patchCord1(waveform1, 0, svf1, 0);
AudioConnection               patchCord2(svf1, 0, i2s1, 0);    // Lowpass output
AudioConnection               patchCord3(svf1, 0, i2s1, 1);    // Lowpass output
// Note: svf1 has 3 outputs: 0=lowpass, 1=bandpass, 2=highpass

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(15);

  // Configure sound source - sawtooth for rich harmonics
  waveform1.begin(0.4, 110, WAVEFORM_SAWTOOTH);

  // Configure state variable filter
  svf1.frequency(1000);     // Filter frequency
  svf1.resonance(2.5);      // Q/resonance (0.7 to 5.0)

  // Start I2S output
  i2s1.begin();

  Serial.println("State Variable Filter Example");
  Serial.println("Sweeps filter frequency with resonance");
  Serial.println("Listening to lowpass output");
}

void loop() {
  // Sweep filter frequency
  static uint32_t next = millis() + 50;
  static float freq = 200.0;
  static float direction = 20.0;

  if (millis() >= next) {
    next = millis() + 50;

    freq += direction;

    if (freq >= 4000.0) {
      direction = -20.0;
    } else if (freq <= 200.0) {
      direction = 20.0;
    }

    svf1.frequency(freq);

    // Print every second
    static uint32_t printNext = millis() + 1000;
    if (millis() >= printNext) {
      printNext = millis() + 1000;
      Serial.print("Filter frequency: ");
      Serial.print(freq);
      Serial.println(" Hz");
    }
  }

  // Vary resonance
  static uint32_t resNext = millis() + 3000;
  static float resonance = 0.7;

  if (millis() >= resNext) {
    resNext = millis() + 3000;

    resonance += 0.5;
    if (resonance > 4.5) resonance = 0.7;

    svf1.resonance(resonance);

    Serial.print("Resonance: ");
    Serial.println(resonance);
  }
}
