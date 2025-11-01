// Play Queue Example
// Demonstrates AudioPlayQueue
// Allows dynamic audio generation and playback from code

#include <pico-audio.h>

// Create audio components
AudioPlayQueue        queue1;        // Audio queue
AudioOutputI2S        i2s1;          // I2S output to PCM5102

AudioConnection       patchCord1(queue1, 0, i2s1, 0);    // Left
AudioConnection       patchCord2(queue1, 0, i2s1, 1);    // Right

void setup() {
  Serial.begin(115200);

  // Allocate audio memory
  AudioMemory(15);

  // Start I2S output
  i2s1.begin();

  Serial.println("Play Queue Example");
  Serial.println("Generates audio samples on-the-fly");
  Serial.println("Creating simple synthesized tones");
}

// Generate a simple tone
void generateTone(int16_t* buffer, float frequency, float amplitude, int samples) {
  static float phase = 0.0;
  float phaseIncrement = frequency * 2.0 * PI / 44100.0;

  for (int i = 0; i < samples; i++) {
    buffer[i] = (int16_t)(sin(phase) * amplitude * 32767);
    phase += phaseIncrement;

    // Keep phase in range
    if (phase >= 2.0 * PI) {
      phase -= 2.0 * PI;
    }
  }
}

void loop() {
  // Check if queue has space for more audio
  int16_t* buffer = queue1.getBuffer();

  if (buffer != NULL) {
    // Generate 128 samples of audio (one block)
    static float frequency = 220.0;
    static bool noteOn = true;

    if (noteOn) {
      generateTone(buffer, frequency, 0.5, 128);
    } else {
      // Silence
      memset(buffer, 0, 128 * sizeof(int16_t));
    }

    // Release buffer for playback
    queue1.playBuffer();

    // Change frequency and note on/off state
    static uint32_t next = millis() + 500;
    if (millis() >= next) {
      next = millis() + 500;

      if (noteOn) {
        noteOn = false;
        Serial.println("Note OFF");
      } else {
        noteOn = true;

        // Play next note in sequence
        static const float notes[] = {220.0, 246.94, 261.63, 293.66, 329.63, 349.23, 392.0, 440.0};
        static int noteIndex = 0;

        frequency = notes[noteIndex];
        noteIndex = (noteIndex + 1) % 8;

        Serial.print("Note ON: ");
        Serial.print(frequency);
        Serial.println(" Hz");
      }
    }
  }

  // Print diagnostics
  static uint32_t diagNext = millis() + 3000;
  if (millis() >= diagNext) {
    diagNext = millis() + 3000;

    Serial.print("CPU: ");
    Serial.print(AudioProcessorUsageMax());
    Serial.print("% Memory: ");
    Serial.println(AudioMemoryUsageMax());

    AudioProcessorUsageMaxReset();
  }
}
