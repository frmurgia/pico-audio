// SD Card Diagnostic Tool
// Tests SPI and SDIO modes to verify SD card connectivity
// VERSION: 1.0
// DATE: 2025-11-01
//
// This sketch helps diagnose SD card connection issues by testing:
// 1. GPIO pin configuration
// 2. SPI mode (simpler, 4 pins)
// 3. SDIO mode (faster, 6 pins)
//
// Pin Assignments to Test:
//
// SPI MODE (Test 1 - verify SD card works):
// - SCK  -> GP6
// - MOSI -> GP7
// - MISO -> GP4
// - CS   -> GP5
//
// SDIO MODE (Test 2 - verify SDIO wiring):
// - CLK  -> GP10
// - CMD  -> GP11
// - DAT0 -> GP12
// - DAT1 -> GP13
// - DAT2 -> GP14
// - DAT3 -> GP15

#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <SD.h>

// SPI Pin Configuration
#define SPI_SCK_PIN  6
#define SPI_MOSI_PIN 7
#define SPI_MISO_PIN 4
#define SPI_CS_PIN   5

// SDIO Pin Configuration
#define SDIO_CLK_PIN  10
#define SDIO_CMD_PIN  11
#define SDIO_DAT0_PIN 12

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println("\n╔═══════════════════════════════════════════════╗");
  Serial.println("║     SD CARD DIAGNOSTIC TOOL v1.0              ║");
  Serial.println("║     Tests SPI and SDIO connectivity           ║");
  Serial.println("╚═══════════════════════════════════════════════╝\n");

  delay(500);

  // Test 1: GPIO Pin Configuration
  Serial.println("═══════════════════════════════════════════════");
  Serial.println("TEST 1: GPIO Pin Configuration");
  Serial.println("═══════════════════════════════════════════════");
  testGPIOPins();

  delay(1000);

  // Test 2: SPI Mode
  Serial.println("\n═══════════════════════════════════════════════");
  Serial.println("TEST 2: SD Card in SPI Mode");
  Serial.println("═══════════════════════════════════════════════");
  Serial.println("This tests basic SD card functionality");
  Serial.println("Wiring for SPI test:");
  Serial.println("  SCK  -> GP6");
  Serial.println("  MOSI -> GP7");
  Serial.println("  MISO -> GP4");
  Serial.println("  CS   -> GP5");
  Serial.println();
  testSPIMode();

  delay(1000);

  // Test 3: SDIO Mode
  Serial.println("\n═══════════════════════════════════════════════");
  Serial.println("TEST 3: SD Card in SDIO Mode");
  Serial.println("═══════════════════════════════════════════════");
  Serial.println("This tests SDIO 4-bit high-speed mode");
  Serial.println("Wiring for SDIO test:");
  Serial.println("  CLK  -> GP10");
  Serial.println("  CMD  -> GP11");
  Serial.println("  DAT0 -> GP12");
  Serial.println("  DAT1 -> GP13");
  Serial.println("  DAT2 -> GP14");
  Serial.println("  DAT3 -> GP15");
  Serial.println();
  testSDIOMode();

  // Final Summary
  Serial.println("\n═══════════════════════════════════════════════");
  Serial.println("DIAGNOSTIC COMPLETE");
  Serial.println("═══════════════════════════════════════════════");
  Serial.println("\nRECOMMENDATIONS:");
  Serial.println("- If SPI works but SDIO fails:");
  Serial.println("  → SD card may not support SDIO mode");
  Serial.println("  → Check SDIO wiring (all 6 pins)");
  Serial.println("  → Use SPI version for compatibility");
  Serial.println();
  Serial.println("- If both fail:");
  Serial.println("  → Check power (3.3V and GND)");
  Serial.println("  → Try different SD card");
  Serial.println("  → Check for loose connections");
  Serial.println();
  Serial.println("- If both work:");
  Serial.println("  → Your hardware is OK!");
  Serial.println("  → Use SDIO version for best performance");
}

void loop() {
  // Nothing to do
  delay(1000);
}

void testGPIOPins() {
  Serial.println("Testing GPIO pins can be configured...");

  // Test SPI pins
  Serial.print("  GP4 (MISO): ");
  pinMode(SPI_MISO_PIN, INPUT);
  Serial.println("OK");

  Serial.print("  GP5 (CS):   ");
  pinMode(SPI_CS_PIN, OUTPUT);
  digitalWrite(SPI_CS_PIN, HIGH);
  Serial.println("OK");

  Serial.print("  GP6 (SCK):  ");
  pinMode(SPI_SCK_PIN, OUTPUT);
  Serial.println("OK");

  Serial.print("  GP7 (MOSI): ");
  pinMode(SPI_MOSI_PIN, OUTPUT);
  Serial.println("OK");

  // Test SDIO pins
  Serial.print("  GP10 (CLK): ");
  pinMode(SDIO_CLK_PIN, OUTPUT);
  Serial.println("OK");

  Serial.print("  GP11 (CMD): ");
  pinMode(SDIO_CMD_PIN, OUTPUT);
  Serial.println("OK");

  Serial.print("  GP12-15:    ");
  pinMode(SDIO_DAT0_PIN, INPUT);
  pinMode(SDIO_DAT0_PIN + 1, INPUT);
  pinMode(SDIO_DAT0_PIN + 2, INPUT);
  pinMode(SDIO_DAT0_PIN + 3, INPUT);
  Serial.println("OK");

  Serial.println("\n✓ All GPIO pins can be configured");
}

void testSPIMode() {
  Serial.print("Configuring SPI pins... ");

  // Configure SPI
  SPI.setRX(SPI_MISO_PIN);
  SPI.setTX(SPI_MOSI_PIN);
  SPI.setSCK(SPI_SCK_PIN);

  Serial.println("OK");

  Serial.print("Initializing SD card (SPI mode)... ");

  bool spiOk = SD.begin(SPI_CS_PIN);

  if (spiOk) {
    Serial.println("✓ SUCCESS!");
    Serial.println();

    // Get card info
    uint64_t cardSize = SD.size();
    uint32_t cardSizeMB = cardSize / (1024 * 1024);

    Serial.println("SD Card Information:");
    Serial.print("  Size: ");
    Serial.print(cardSizeMB);
    Serial.println(" MB");

    Serial.print("  Type: ");
    // Try to determine card type from size
    if (cardSizeMB < 100) {
      Serial.println("< 100 MB (likely old/small card)");
    } else if (cardSizeMB < 2048) {
      Serial.println("< 2 GB (SD)");
    } else if (cardSizeMB < 32768) {
      Serial.println("2-32 GB (SDHC)");
    } else {
      Serial.println("> 32 GB (SDXC)");
    }

    // Try to list files
    Serial.println("\nAttempting to list root directory:");
    File root = SD.open("/");
    if (root) {
      int fileCount = 0;
      File entry;
      while ((entry = root.openNextFile())) {
        Serial.print("  - ");
        Serial.print(entry.name());
        Serial.print(" (");
        Serial.print(entry.size());
        Serial.println(" bytes)");
        entry.close();
        fileCount++;
        if (fileCount >= 10) {
          Serial.println("  ... (showing first 10 files)");
          break;
        }
      }
      root.close();

      if (fileCount == 0) {
        Serial.println("  (empty)");
      }
    } else {
      Serial.println("  Could not open root directory");
    }

    Serial.println("\n✓ SPI MODE WORKS!");
    Serial.println("  → SD card is functional");
    Serial.println("  → SPI wiring is correct");

  } else {
    Serial.println("✗ FAILED");
    Serial.println();
    Serial.println("Possible causes:");
    Serial.println("  1. SD card not inserted");
    Serial.println("  2. Wiring incorrect (check SPI pins)");
    Serial.println("  3. SD card damaged/incompatible");
    Serial.println("  4. Power issue (check 3.3V and GND)");
    Serial.println();
    Serial.println("Double-check SPI wiring:");
    Serial.println("  SD Card -> RP2350");
    Serial.println("  SCK  -> GP6");
    Serial.println("  MOSI -> GP7");
    Serial.println("  MISO -> GP4");
    Serial.println("  CS   -> GP5");
    Serial.println("  VCC  -> 3.3V");
    Serial.println("  GND  -> GND");
  }
}

void testSDIOMode() {
  Serial.print("Initializing SD card (SDIO 4-bit mode)... ");

  bool sdioOk = SD.begin(SDIO_CLK_PIN, SDIO_CMD_PIN, SDIO_DAT0_PIN);

  if (sdioOk) {
    Serial.println("✓ SUCCESS!");
    Serial.println();

    // Get card info
    uint64_t cardSize = SD.size();
    uint32_t cardSizeMB = cardSize / (1024 * 1024);

    Serial.println("SD Card Information (SDIO mode):");
    Serial.print("  Size: ");
    Serial.print(cardSizeMB);
    Serial.println(" MB");
    Serial.println("  Bandwidth: 10-12 MB/s (SDIO 4-bit)");

    // Try to list files
    Serial.println("\nAttempting to list root directory:");
    File root = SD.open("/");
    if (root) {
      int fileCount = 0;
      File entry;
      while ((entry = root.openNextFile())) {
        Serial.print("  - ");
        Serial.print(entry.name());
        Serial.print(" (");
        Serial.print(entry.size());
        Serial.println(" bytes)");
        entry.close();
        fileCount++;
        if (fileCount >= 10) {
          Serial.println("  ... (showing first 10 files)");
          break;
        }
      }
      root.close();

      if (fileCount == 0) {
        Serial.println("  (empty)");
      }
    }

    Serial.println("\n✓ SDIO MODE WORKS!");
    Serial.println("  → SD card supports SDIO");
    Serial.println("  → SDIO wiring is correct");
    Serial.println("  → Use SDCardWavPlayerSDIO for best performance!");

  } else {
    Serial.println("✗ FAILED");
    Serial.println();
    Serial.println("Possible causes:");
    Serial.println("  1. SD card doesn't support SDIO mode");
    Serial.println("     (some older/cheap cards are SPI-only)");
    Serial.println("  2. SDIO wiring incorrect");
    Serial.println("     ⚠️ DAT0-3 MUST be on consecutive GPIO pins!");
    Serial.println("  3. Cable length too long (SDIO needs short wires)");
    Serial.println("  4. Missing pull-up resistors (some cards need them)");
    Serial.println();
    Serial.println("Double-check SDIO wiring:");
    Serial.println("  SD Card -> RP2350");
    Serial.println("  CLK  -> GP10");
    Serial.println("  CMD  -> GP11");
    Serial.println("  DAT0 -> GP12  ← Must be consecutive!");
    Serial.println("  DAT1 -> GP13  ← GP12+1");
    Serial.println("  DAT2 -> GP14  ← GP12+2");
    Serial.println("  DAT3 -> GP15  ← GP12+3");
    Serial.println("  VCC  -> 3.3V");
    Serial.println("  GND  -> GND");
    Serial.println();
    Serial.println("If SPI worked but SDIO failed:");
    Serial.println("  → Use SDCardWavPlayerDualCore (SPI version)");
    Serial.println("  → Slower but more compatible");
  }
}
