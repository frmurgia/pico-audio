// Bitcrusher Effect Example
// Demonstrates AudioEffectBitcrusher
// Reduces bit depth and sample rate for lo-fi digital distortion

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform       waveform1;      // Sound source
AudioEffectBitcrusher    bitcrusher1;    // Bitcrusher effect
AudioOutputI2S           i2s1;           // I2S output to PCM5102

AudioConnection          patchCord1(waveform1, 0, bitcrusher1, 0);
AudioConnection          patchCord2(bitcrusher1, 0, i2s1, 0);      // Left output
AudioConnection          patchCord3(bitcrusher1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(10);

  // Configure sound source - slow frequency sweep
  waveform1.begin(0.5, 220, WAVEFORM_SAWTOOTH);

  // Configure bitcrusher
  bitcrusher1.bits(8);         // Bit depth (1-16)
  bitcrusher1.sampleRate(11025); // Sample rate in Hz

  // Start I2S output
  i2s1.begin();

  Serial.println("Bitcrusher Lo-Fi Effect");
  Serial.println("Reduces bit depth and sample rate");
  Serial.println("Sweeping through bit depths");
}

void loop() {
  // Sweep frequency
  static uint32_t freqNext = millis() + 50;
  static float freq = 220.0;
  static float direction = 5.0;

  if (millis() >= freqNext) {
    freqNext = millis() + 50;
    freq += direction;
    if (freq >= 880.0) direction = -5.0;
    if (freq <= 220.0) direction = 5.0;
    waveform1.frequency(freq);
  }

  // Change bit depth every 2 seconds
  static uint32_t bitNext = millis() + 2000;
  static int bits = 16;

  if (millis() >= bitNext) {
    bitNext = millis() + 2000;

    bits -= 2;
    if (bits < 2) bits = 16;

    bitcrusher1.bits(bits);

    Serial.print("Bit depth: ");
    Serial.print(bits);
    Serial.println(" bits");
  }
}
