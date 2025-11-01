// FIR Filter Example
// Demonstrates AudioFilterFIR
// Finite Impulse Response filter with custom coefficients

#include <pico-audio.h>

// Create audio components
AudioSynthNoiseWhite    noise1;        // White noise source
AudioFilterFIR          fir1;          // FIR filter
AudioOutputI2S          i2s1;          // I2S output to PCM5102

AudioConnection         patchCord1(noise1, 0, fir1, 0);
AudioConnection         patchCord2(fir1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(fir1, 0, i2s1, 1);      // Right output

// Simple lowpass FIR filter coefficients (21-tap)
// Generated for cutoff at ~1000 Hz
const short firCoefficients[21] = {
  -53, -108, -56, 131, 448, 738, 838, 641, 181, -393,
  -809, -809, -393, 181, 641, 838, 738, 448, 131, -56,
  -108
};

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(15);

  // Configure noise source
  noise1.amplitude(0.4);

  // Load FIR coefficients
  fir1.begin(firCoefficients, 21);

  // Start I2S output
  i2s1.begin();

  Serial.println("FIR Filter Example");
  Serial.println("21-tap lowpass FIR filter (~1000 Hz cutoff)");
  Serial.println("Filtering white noise");
  Serial.println("FIR filters have linear phase response");
}

void loop() {
  // Print CPU usage every 3 seconds
  static uint32_t next = millis() + 3000;

  if (millis() >= next) {
    next = millis() + 3000;

    Serial.print("CPU: ");
    Serial.print(AudioProcessorUsageMax());
    Serial.print("% Memory: ");
    Serial.println(AudioMemoryUsageMax());

    AudioProcessorUsageMaxReset();
  }
}
