// 4-Channel Mixer Example
// Demonstrates AudioMixer4
// Mixes up to 4 audio sources with independent gain control

#include <pico-audio.h>

// Create audio components
AudioSynthWaveform    osc1;          // Oscillator 1
AudioSynthWaveform    osc2;          // Oscillator 2
AudioSynthWaveform    osc3;          // Oscillator 3
AudioSynthWaveform    osc4;          // Oscillator 4
AudioMixer4           mixer1;        // 4-channel mixer
AudioOutputI2S        i2s1;          // I2S output to PCM5102

// Connect all oscillators to mixer inputs
AudioConnection       patchCord1(osc1, 0, mixer1, 0);
AudioConnection       patchCord2(osc2, 0, mixer1, 1);
AudioConnection       patchCord3(osc3, 0, mixer1, 2);
AudioConnection       patchCord4(osc4, 0, mixer1, 3);

// Connect mixer output to I2S
AudioConnection       patchCord5(mixer1, 0, i2s1, 0);    // Left
AudioConnection       patchCord6(mixer1, 0, i2s1, 1);    // Right

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(15);

  // Configure oscillators - major chord (C E G C)
  osc1.begin(0.3, 261.63, WAVEFORM_SINE);      // C4
  osc2.begin(0.3, 329.63, WAVEFORM_SINE);      // E4
  osc3.begin(0.3, 392.00, WAVEFORM_SINE);      // G4
  osc4.begin(0.3, 523.25, WAVEFORM_SINE);      // C5

  // Set mixer gains (0.0 to 1.0, or higher for amplification)
  mixer1.gain(0, 0.25);  // Channel 0 (osc1)
  mixer1.gain(1, 0.25);  // Channel 1 (osc2)
  mixer1.gain(2, 0.25);  // Channel 2 (osc3)
  mixer1.gain(3, 0.25);  // Channel 3 (osc4)

  // Start I2S output
  i2s1.begin();

  Serial.println("4-Channel Mixer Example");
  Serial.println("Mixing 4 oscillators playing a C major chord");
  Serial.println("Individual channel gains will vary");
}

void loop() {
  // Vary individual channel gains to create movement
  static uint32_t next = millis() + 100;
  static float phase = 0.0;

  if (millis() >= next) {
    next = millis() + 100;

    phase += 0.1;

    // Use sine waves to modulate gain of each channel
    mixer1.gain(0, 0.15 + 0.15 * sin(phase));
    mixer1.gain(1, 0.15 + 0.15 * sin(phase + 1.57));       // 90° phase
    mixer1.gain(2, 0.15 + 0.15 * sin(phase + 3.14));       // 180° phase
    mixer1.gain(3, 0.15 + 0.15 * sin(phase + 4.71));       // 270° phase
  }

  // Print mixer info every 2 seconds
  static uint32_t printNext = millis() + 2000;
  if (millis() >= printNext) {
    printNext = millis() + 2000;

    Serial.println("Mixer channels fading in and out");
    Serial.print("CPU: ");
    Serial.print(AudioProcessorUsageMax());
    Serial.println("%");

    AudioProcessorUsageMaxReset();
  }
}
