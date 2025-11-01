// Audio Print Example
// Demonstrates AudioAnalyzePrint
// Prints raw audio data to serial for debugging and analysis

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Create audio components
AudioSynthWaveform      waveform1;      // Sound source
AudioAnalyzePrint       print1;         // Audio data printer
AudioOutputI2S          i2s1;           // I2S output to PCM5102

AudioConnection         patchCord1(waveform1, 0, print1, 0);
AudioConnection         patchCord2(waveform1, 0, i2s1, 0);      // Left output
AudioConnection         patchCord3(waveform1, 0, i2s1, 1);      // Right output

void setup() {
  Serial.begin(115200);

  // Wait for serial connection
  while (!Serial && millis() < 3000) {
    delay(100);
  }

  // Allocate audio memory
  AudioMemory(15);

  // Configure sound source - low frequency for easy viewing
  waveform1.begin(0.5, 50, WAVEFORM_SINE);  // 50 Hz sine

  // Start I2S output
  i2s1.begin();

  Serial.println("\n=== Audio Print Debug Tool ===");
  Serial.println("Printing raw audio samples");
  Serial.println("50 Hz sine wave");
  Serial.println("Sample rate: 44100 Hz, Block size: 128 samples");
  Serial.println("================================\n");

  delay(1000);

  // Trigger print - prints one block of audio data
  print1.trigger();
}

void loop() {
  // Trigger print every 2 seconds
  static uint32_t next = millis() + 2000;

  if (millis() >= next) {
    next = millis() + 2000;

    Serial.println("\n--- Triggering new print ---");
    print1.trigger();

    // Print diagnostics
    Serial.print("CPU: ");
    Serial.print(AudioProcessorUsageMax());
    Serial.print("% Memory: ");
    Serial.println(AudioMemoryUsageMax());

    AudioProcessorUsageMaxReset();
  }

  // Change waveform type occasionally
  static uint32_t waveNext = millis() + 8000;
  static int waveType = WAVEFORM_SINE;

  if (millis() >= waveNext) {
    waveNext = millis() + 8000;

    switch (waveType) {
      case WAVEFORM_SINE:
        waveType = WAVEFORM_SQUARE;
        waveform1.begin(0.5, 50, WAVEFORM_SQUARE);
        Serial.println("\n>>> Changing to SQUARE wave <<<\n");
        break;
      case WAVEFORM_SQUARE:
        waveType = WAVEFORM_SINE;
        waveform1.begin(0.5, 50, WAVEFORM_SINE);
        Serial.println("\n>>> Changing to SINE wave <<<\n");
        break;
    }
  }
}
