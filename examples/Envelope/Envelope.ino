// ADSR Envelope Example
// Demonstrates AudioEffectEnvelope
// Attack-Decay-Sustain-Release envelope shaping

#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      waveform1;      // Sound source
AudioEffectEnvelope     envelope1;      // ADSR envelope
AudioOutputI2S          i2s1;           // I2S output to PCM5102

AudioConnection         patchCord1(waveform1, envelope1);
AudioConnection         patchCord2(envelope1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(envelope1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(10);

  // Configure sound source (constant tone)
  waveform1.begin(1.0, 440, WAVEFORM_SAWTOOTH);

  // Configure ADSR envelope (all times in milliseconds)
  envelope1.attack(100);      // Attack time
  envelope1.decay(200);       // Decay time
  envelope1.sustain(0.6);     // Sustain level (0.0 to 1.0)
  envelope1.release(500);     // Release time

  // Start I2S output
  i2s1.begin();

  Serial.println("ADSR Envelope Example");
  Serial.println("Attack: 100ms, Decay: 200ms, Sustain: 0.6, Release: 500ms");
  Serial.println("Press notes with envelope shaping");
}

void loop() {
  // Play notes with envelope
  static uint32_t next = millis() + 2000;
  static bool noteActive = false;
  static const float notes[] = {261.63, 329.63, 392.00, 523.25};  // C, E, G, C
  static int noteIndex = 0;

  if (millis() >= next) {
    if (!noteActive) {
      // Note on
      waveform1.frequency(notes[noteIndex]);
      envelope1.noteOn();

      Serial.print("Note ON: ");
      Serial.print(notes[noteIndex]);
      Serial.println(" Hz");

      noteActive = true;
      next = millis() + 800;  // Hold note for 800ms
    } else {
      // Note off
      envelope1.noteOff();

      Serial.println("Note OFF - releasing");

      noteActive = false;
      next = millis() + 1200;  // Wait for release + gap

      noteIndex = (noteIndex + 1) % 4;
    }
  }
}
