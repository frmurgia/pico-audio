// I2S Audio Test - PCM5102 DAC
// Generates test tones to verify I2S output configuration
//
// Use this to diagnose I2S connection issues
//
// Expected PCM5102 connections:
// - BCK (Bit Clock)    → Check AudioOutputI2S.h for default pin
// - LCK (Word Select)  → Check AudioOutputI2S.h for default pin
// - DIN (Data)         → Check AudioOutputI2S.h for default pin
// - VIN  → 3.3V
// - GND  → GND
// - SCK  → GND (use Pico clock)
// - FMT  → GND (I2S format)
// - XMT  → 3.3V or floating (unmute)

#include <Adafruit_TinyUSB.h>
#include <pico-audio.h>

// Test tone generator
AudioSynthWaveformSine   sine1;       // 440 Hz test tone
AudioOutputI2S           i2s1;        // I2S output
AudioConnection          patchCord1(sine1, 0, i2s1, 0);  // Left
AudioConnection          patchCord2(sine1, 0, i2s1, 1);  // Right

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 3000)) {
    delay(100);
  }

  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   I2S Audio Output Test            ║");
  Serial.println("║   PCM5102 DAC Verification         ║");
  Serial.println("╚════════════════════════════════════╝\n");

  // Allocate audio memory
  AudioMemory(10);

  Serial.println("Starting I2S output...");
  i2s1.begin();
  Serial.println("✓ I2S initialized");

  // Configure test tone - 440 Hz (A4)
  sine1.amplitude(0.5);      // 50% volume
  sine1.frequency(440);      // A4 note

  Serial.println("✓ Test tone configured: 440 Hz sine wave");
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║  YOU SHOULD HEAR A TONE NOW!       ║");
  Serial.println("║  440 Hz (A4 musical note)          ║");
  Serial.println("╚════════════════════════════════════╝\n");

  Serial.println("If you hear NOTHING, check:");
  Serial.println("  1. PCM5102 wiring (BCK, LCK, DIN pins)");
  Serial.println("  2. PCM5102 power (3.3V on VIN)");
  Serial.println("  3. PCM5102 config pins:");
  Serial.println("     - SCK → GND");
  Serial.println("     - FMT → GND");
  Serial.println("     - XMT → 3.3V or floating (NOT GND)");
  Serial.println("  4. Output connected to speaker/headphones");
  Serial.println("  5. Check AudioOutputI2S.h for I2S pin configuration");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  1-9 : Change frequency (100-900 Hz)");
  Serial.println("  0   : 440 Hz (A4)");
  Serial.println("  +   : Increase volume");
  Serial.println("  -   : Decrease volume");
  Serial.println("  s   : Silence");
  Serial.println();
}

float currentVolume = 0.5;
float currentFreq = 440;

void loop() {
  // Handle commands
  if (Serial.available()) {
    char cmd = Serial.read();

    if (cmd >= '1' && cmd <= '9') {
      currentFreq = (cmd - '0') * 100;
      sine1.frequency(currentFreq);
      Serial.print("Frequency: ");
      Serial.print(currentFreq);
      Serial.println(" Hz");
    }
    else if (cmd == '0') {
      currentFreq = 440;
      sine1.frequency(currentFreq);
      Serial.println("Frequency: 440 Hz (A4)");
    }
    else if (cmd == '+') {
      currentVolume += 0.1;
      if (currentVolume > 1.0) currentVolume = 1.0;
      sine1.amplitude(currentVolume);
      Serial.print("Volume: ");
      Serial.print(currentVolume * 100);
      Serial.println("%");
    }
    else if (cmd == '-') {
      currentVolume -= 0.1;
      if (currentVolume < 0.0) currentVolume = 0.0;
      sine1.amplitude(currentVolume);
      Serial.print("Volume: ");
      Serial.print(currentVolume * 100);
      Serial.println("%");
    }
    else if (cmd == 's' || cmd == 'S') {
      currentVolume = 0.0;
      sine1.amplitude(currentVolume);
      Serial.println("SILENCE");
    }
  }

  // Print status every 3 seconds
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 3000) {
    lastPrint = millis();

    Serial.print("♪ Freq: ");
    Serial.print(currentFreq);
    Serial.print(" Hz | Vol: ");
    Serial.print(currentVolume * 100);
    Serial.print("% | CPU: ");
    Serial.print(AudioProcessorUsageMax());
    Serial.print("% | Mem: ");
    Serial.println(AudioMemoryUsageMax());

    AudioProcessorUsageMaxReset();
  }
}
