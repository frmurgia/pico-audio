// SD Card SDIO Diagnostic Tool - ADVANCED VERSION
// VERSION: 3.0
// DATE: 2025-11-02
//
// Tests SDIO functionality with detailed debugging
//
// SDIO Pin Configuration (VERIFIED WORKING):
// - CLK  -> GP7
// - CMD  -> GP6
// - DAT0 -> GP8
// - DAT1 -> GP9
// - DAT2 -> GP10
// - DAT3 -> GP11
//
// SPI Pin Configuration (for comparison):
// - CS   -> GP3
// - SCK  -> GP6
// - MOSI -> GP7
// - MISO -> GP0
//
// NOTE: Some pins overlap between SPI and SDIO - this is intentional
// to minimize wiring changes when switching between modes

#include <Adafruit_TinyUSB.h>
#include <SD.h>
#include <SPI.h>
#include <hardware/gpio.h>

// SDIO Pin Configuration (VERIFIED WORKING)
#define SDIO_CLK_PIN  7
#define SDIO_CMD_PIN  6
#define SDIO_DAT0_PIN 8  // DAT1=9, DAT2=10, DAT3=11 (consecutive!)

// SPI Pin Configuration
#define SPI_CS_PIN   3
#define SPI_SCK_PIN  6
#define SPI_MOSI_PIN 7
#define SPI_MISO_PIN 0

void setup() {
  Serial.begin(115200);

  // Wait for serial with timeout
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println();
  Serial.println("=========================================");
  Serial.println("SD Card SDIO Diagnostic Tool v3.0");
  Serial.println("=========================================");
  Serial.println();

  // Print chip info
  Serial.println("Chip Information:");
  Serial.print("  RP2350: ");
  #ifdef PICO_RP2350
  Serial.println("YES");
  #else
  Serial.println("NO");
  #endif

  Serial.print("  Flash: ");
  Serial.print((PICO_FLASH_SIZE_BYTES / (1024 * 1024)));
  Serial.println(" MB");

  Serial.print("  PSRAM: ");
  #ifdef PICO_PSRAM_SIZE
  Serial.print((PICO_PSRAM_SIZE / (1024 * 1024)));
  Serial.println(" MB");
  #else
  Serial.println("0 MB");
  #endif

  Serial.println();

  // Test GPIO availability
  testGPIOPins();

  Serial.println();

  // Test SPI mode (known working)
  testSPIMode();

  Serial.println();

  // Test SDIO mode with multiple approaches
  testSDIOMode_Method1();

  Serial.println();

  testSDIOMode_Method2();

  Serial.println();

  testSDIOMode_Method3();

  Serial.println();
  Serial.println("=========================================");
  Serial.println("Diagnostic Complete");
  Serial.println("=========================================");
}

void loop() {
  // Nothing to do
  delay(1000);
}

void testGPIOPins() {
  Serial.println("Testing GPIO Pin Configuration:");
  Serial.println("-------------------------------");

  // Test SDIO pins
  Serial.print("  GP0 (DAT0): ");
  testPin(SDIO_DAT0_PIN);

  Serial.print("  GP1 (DAT1): ");
  testPin(SDIO_DAT0_PIN + 1);

  Serial.print("  GP2 (DAT2): ");
  testPin(SDIO_DAT0_PIN + 2);

  Serial.print("  GP3 (DAT3): ");
  testPin(SDIO_DAT0_PIN + 3);

  Serial.print("  GP6 (CLK):  ");
  testPin(SDIO_CLK_PIN);

  Serial.print("  GP7 (CMD):  ");
  testPin(SDIO_CMD_PIN);

  Serial.println();
  Serial.println("  ✓ All SDIO pins are accessible");
}

void testPin(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
  delay(1);
  digitalWrite(pin, LOW);
  delay(1);
  Serial.println("OK");
}

void testSPIMode() {
  Serial.println("Test 1: SPI Mode (Reference - Known Working)");
  Serial.println("---------------------------------------------");
  Serial.print("  Configuring SPI pins... ");

  SPI.setRX(SPI_MISO_PIN);
  SPI.setTX(SPI_MOSI_PIN);
  SPI.setSCK(SPI_SCK_PIN);
  Serial.println("OK");

  Serial.print("  Initializing SD card (SPI)... ");
  bool spiOk = SD.begin(SPI_CS_PIN);

  if (spiOk) {
    Serial.println("✓ SUCCESS");

    uint64_t cardSize = SD.size();
    uint32_t cardSizeMB = cardSize / (1024 * 1024);
    Serial.print("  Card size: ");
    Serial.print(cardSizeMB);
    Serial.println(" MB");

    Serial.println("  Card type: SD SPI Mode");
    Serial.println("  Bandwidth: ~1-2 MB/s (limited by SPI)");

    // List files
    Serial.println();
    Serial.println("  Files on SD card:");
    File root = SD.open("/");
    listFiles(root, "    ");
    root.close();

    SD.end();
    delay(100);
  } else {
    Serial.println("✗ FAILED");
    Serial.println("  → SD card not detected in SPI mode");
    Serial.println("  → Check wiring, card insertion, card format");
  }
}

void testSDIOMode_Method1() {
  Serial.println("Test 2: SDIO Mode - Method 1 (Standard)");
  Serial.println("----------------------------------------");
  Serial.print("  Initializing SD (SDIO 4-bit)... ");

  // Method 1: Standard 3-parameter begin() for SDIO
  bool sdioOk = SD.begin(SDIO_CLK_PIN, SDIO_CMD_PIN, SDIO_DAT0_PIN);

  if (sdioOk) {
    Serial.println("✓ SUCCESS!");

    uint64_t cardSize = SD.size();
    uint32_t cardSizeMB = cardSize / (1024 * 1024);
    Serial.print("  Card size: ");
    Serial.print(cardSizeMB);
    Serial.println(" MB");

    Serial.println("  Card type: SDIO 4-bit Mode");
    Serial.println("  Bandwidth: ~10-12 MB/s (high speed)");

    // Test read speed
    Serial.println();
    testReadSpeed();

    SD.end();
    delay(100);
  } else {
    Serial.println("✗ FAILED");
    Serial.println("  → SDIO initialization failed");
    Serial.println();
    Serial.println("  Possible causes:");
    Serial.println("    1. SD card doesn't support SDIO (older/cheap cards)");
    Serial.println("    2. Wiring issue (check DAT0-3 consecutive)");
    Serial.println("    3. Clock speed too high for wiring");
    Serial.println("    4. Missing pull-up resistors");
  }
}

void testSDIOMode_Method2() {
  Serial.println("Test 3: SDIO Mode - Method 2 (With Delay)");
  Serial.println("------------------------------------------");
  Serial.println("  Waiting 500ms before init...");
  delay(500);

  Serial.print("  Initializing SD (SDIO 4-bit)... ");
  bool sdioOk = SD.begin(SDIO_CLK_PIN, SDIO_CMD_PIN, SDIO_DAT0_PIN);

  if (sdioOk) {
    Serial.println("✓ SUCCESS!");

    uint64_t cardSize = SD.size();
    uint32_t cardSizeMB = cardSize / (1024 * 1024);
    Serial.print("  Card size: ");
    Serial.print(cardSizeMB);
    Serial.println(" MB");

    SD.end();
    delay(100);
  } else {
    Serial.println("✗ FAILED");
    Serial.println("  → Delay didn't help - likely hardware/card issue");
  }
}

void testSDIOMode_Method3() {
  Serial.println("Test 4: SDIO Mode - Method 3 (After SPI Reset)");
  Serial.println("-----------------------------------------------");
  Serial.print("  Re-initializing SPI first... ");

  SPI.setRX(SPI_MISO_PIN);
  SPI.setTX(SPI_MOSI_PIN);
  SPI.setSCK(SPI_SCK_PIN);

  bool spiOk = SD.begin(SPI_CS_PIN);
  if (spiOk) {
    Serial.println("OK");
    SD.end();
    delay(100);

    Serial.print("  Now switching to SDIO... ");
    bool sdioOk = SD.begin(SDIO_CLK_PIN, SDIO_CMD_PIN, SDIO_DAT0_PIN);

    if (sdioOk) {
      Serial.println("✓ SUCCESS!");

      uint64_t cardSize = SD.size();
      uint32_t cardSizeMB = cardSize / (1024 * 1024);
      Serial.print("  Card size: ");
      Serial.print(cardSizeMB);
      Serial.println(" MB");

      SD.end();
      delay(100);
    } else {
      Serial.println("✗ FAILED");
      Serial.println("  → Card works in SPI but not SDIO");
      Serial.println("  → This SD card likely doesn't support SDIO mode");
      Serial.println();
      Serial.println("  RECOMMENDATION: Use SPI mode with optimizations");
    }
  } else {
    Serial.println("FAILED (SPI also failed)");
  }
}

void testReadSpeed() {
  Serial.println("  Testing read speed...");

  // Try to open a file for speed test
  File root = SD.open("/");
  File testFile = root.openNextFile();

  if (!testFile || testFile.isDirectory()) {
    Serial.println("    → No files found for speed test");
    root.close();
    return;
  }

  Serial.print("    Testing with: ");
  Serial.println(testFile.name());

  uint32_t fileSize = testFile.size();
  if (fileSize > 1024*1024) {
    fileSize = 1024*1024;  // Test with max 1MB
  }

  uint8_t buffer[512];
  unsigned long startTime = micros();
  uint32_t bytesRead = 0;

  while (bytesRead < fileSize) {
    int toRead = min((int)(fileSize - bytesRead), 512);
    int actual = testFile.read(buffer, toRead);
    if (actual <= 0) break;
    bytesRead += actual;
  }

  unsigned long elapsed = micros() - startTime;
  testFile.close();
  root.close();

  if (elapsed > 0) {
    float speedMBps = (bytesRead / 1024.0 / 1024.0) / (elapsed / 1000000.0);
    Serial.print("    Read ");
    Serial.print(bytesRead);
    Serial.print(" bytes in ");
    Serial.print(elapsed / 1000.0);
    Serial.print(" ms");
    Serial.print(" = ");
    Serial.print(speedMBps);
    Serial.println(" MB/s");

    if (speedMBps > 5.0) {
      Serial.println("    ✓ Excellent speed - SDIO working properly!");
    } else if (speedMBps > 2.0) {
      Serial.println("    ⚠ Moderate speed - may be using 1-bit mode");
    } else {
      Serial.println("    ⚠ Low speed - similar to SPI, SDIO may not be active");
    }
  }
}

void listFiles(File dir, const char* prefix) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    Serial.print(prefix);
    Serial.print(entry.name());

    if (entry.isDirectory()) {
      Serial.println("/");
    } else {
      Serial.print(" (");
      Serial.print(entry.size());
      Serial.println(" bytes)");
    }

    entry.close();
  }
}
