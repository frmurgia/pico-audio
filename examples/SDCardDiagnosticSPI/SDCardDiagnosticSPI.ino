// SD Card Diagnostic Tool - SPI Version
// VERSION: 1.0
// DATE: 2025-11-02
//
// Tool di diagnostica COMPLETA per SD card con SPI
// Verifica SOLO la SD card - niente audio, niente MP3
//
// SPI Pin Configuration:
// - CS   -> GP3
// - SCK  -> GP6
// - MOSI -> GP7
// - MISO -> GP0
//
// Comandi Serial:
// - 'i' : SD card info
// - 'l' : List all files
// - 'r' : Read test (leggi primi 512 bytes di un file)
// - 'w' : Write test (crea file test)
// - 'd' : Delete test file
// - 'f' : Format info
// - 's' : Speed test
// - 'h' : Help

#include <Adafruit_TinyUSB.h>
#include <SD.h>
#include <SPI.h>


// SD SCK → GP18
// SD MOSI → GP19
// SD MISO → GP16
// SD CS → GP17

#define SD_CS_PIN   13    // Chip Select
#define SD_SCK_PIN  10    // Clock
#define SD_MOSI_PIN 11    // Master Out Slave In
#define SD_MISO_PIN 12    // Master In Slave Out1

void setup() {
  Serial.begin(115200);

  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println("\n╔═══════════════════════════════════════════╗");
  Serial.println("║  SD CARD DIAGNOSTIC TOOL - SPI            ║");
  Serial.println("║  v1.0 - Complete SD Card Testing          ║");
  Serial.println("╚═══════════════════════════════════════════╝\n");

  Serial.println("This tool tests ONLY the SD card connection.");
  Serial.println("No audio, no MP3 - just pure SD diagnostics.\n");

  // Configure SPI pins
  Serial.println("╔═══ SPI CONFIGURATION ═══╗");
  Serial.print("  Setting up SPI... ");

  SPI.setRX(SD_MISO_PIN);
  SPI.setTX(SD_MOSI_PIN);
  SPI.setSCK(SD_SCK_PIN);

  Serial.println("OK");
  Serial.println("\n  Pin Configuration:");
  Serial.print("    CS   -> GP"); Serial.println(SD_CS_PIN);
  Serial.print("    SCK  -> GP"); Serial.println(SD_SCK_PIN);
  Serial.print("    MOSI -> GP"); Serial.println(SD_MOSI_PIN);
  Serial.print("    MISO -> GP"); Serial.println(SD_MISO_PIN);
  Serial.println();

  // Initialize SD card
  Serial.println("╔═══ SD CARD INITIALIZATION ═══╗");
  Serial.print("  Initializing SD card... ");

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("❌ FAILED!\n");
    Serial.println("  ERROR: Cannot initialize SD card\n");
    Serial.println("  Troubleshooting checklist:");
    Serial.println("  □ SD card is inserted correctly");
    Serial.println("  □ SD card is formatted (FAT32 recommended)");
    Serial.println("  □ Check wiring:");
    Serial.println("      CS   (SD pin 1) -> GP3");
    Serial.println("      SCK  (SD pin 5) -> GP6");
    Serial.println("      MOSI (SD pin 2) -> GP7");
    Serial.println("      MISO (SD pin 7) -> GP0");
    Serial.println("      VCC  (SD pin 4) -> 3.3V");
    Serial.println("      GND  (SD pin 3) -> GND");
    Serial.println("  □ Verify 3.3V on SD card VCC pin");
    Serial.println("  □ Try a different SD card");
    Serial.println("  □ Check SD card adapter quality");
    Serial.println();
    Serial.println("  Press RESET button on Pico to retry...");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("✓ SUCCESS!\n");

  // Get card info
  uint64_t cardSize = SD.size();
  uint32_t cardSizeMB = cardSize / (1024 * 1024);

  Serial.println("  Card Information:");
  Serial.print("    Size: ");
  Serial.print(cardSizeMB);
  Serial.println(" MB");

  // Detect card type by size
  Serial.print("    Type: ");
  if (cardSize < 2ULL * 1024 * 1024 * 1024) {
    Serial.println("SD/SDHC (< 2GB)");
  } else if (cardSize < 32ULL * 1024 * 1024 * 1024) {
    Serial.println("SDHC (2-32GB)");
  } else {
    Serial.println("SDXC (> 32GB)");
  }

  Serial.println();

  // Show space
  showSpace();

  // Auto list files
  Serial.println("╔═══ FILE LISTING ═══╗");
  listFiles("/", true);

  Serial.println("\n╔═══════════════════════════════════════════╗");
  Serial.println("║  READY - Type 'h' for help               ║");
  Serial.println("╚═══════════════════════════════════════════╝\n");
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    while (Serial.available()) Serial.read(); // Clear buffer

    Serial.println();

    switch (cmd) {
      case 'i':
      case 'I':
        showInfo();
        break;

      case 'l':
      case 'L':
        Serial.println("╔═══ FILE LISTING ═══╗");
        listFiles("/", true);
        break;

      case 'r':
      case 'R':
        readTest();
        break;

      case 'w':
      case 'W':
        writeTest();
        break;

      case 'd':
      case 'D':
        deleteTest();
        break;

      case 'f':
      case 'F':
        formatInfo();
        break;

      case 's':
      case 'S':
        speedTest();
        break;

      case 'h':
      case 'H':
        showHelp();
        break;

      default:
        Serial.println("Unknown command. Type 'h' for help.");
        break;
    }

    Serial.println();
  }

  delay(10);
}

void showHelp() {
  Serial.println("╔═══ COMMAND HELP ═══╗");
  Serial.println("  i - SD card info (size, type, space)");
  Serial.println("  l - List all files on SD card");
  Serial.println("  r - Read test (read first file)");
  Serial.println("  w - Write test (create test.txt)");
  Serial.println("  d - Delete test file");
  Serial.println("  f - Format information");
  Serial.println("  s - Speed test (read/write)");
  Serial.println("  h - This help");
}

void showInfo() {
  Serial.println("╔═══ SD CARD INFO ═══╗");

  uint64_t cardSize = SD.size();
  uint32_t cardSizeMB = cardSize / (1024 * 1024);

  Serial.print("  Card size: ");
  Serial.print(cardSizeMB);
  Serial.println(" MB");

  Serial.print("  Card type: ");
  if (cardSize < 2ULL * 1024 * 1024 * 1024) {
    Serial.println("SD/SDHC (< 2GB)");
  } else if (cardSize < 32ULL * 1024 * 1024 * 1024) {
    Serial.println("SDHC (2-32GB)");
  } else {
    Serial.println("SDXC (> 32GB)");
  }

  Serial.println();
  showSpace();
}

void showSpace() {
  // Count used space
  uint64_t usedBytes = 0;
  File root = SD.open("/");
  if (root) {
    countSpace(root, usedBytes);
    root.close();
  }

  uint64_t totalBytes = SD.size();
  uint64_t freeBytes = totalBytes - usedBytes;

  Serial.println("  Space:");
  Serial.print("    Total: ");
  Serial.print((uint32_t)(totalBytes / (1024 * 1024)));
  Serial.println(" MB");

  Serial.print("    Used:  ");
  Serial.print((uint32_t)(usedBytes / (1024 * 1024)));
  Serial.print(" MB (");
  Serial.print((usedBytes * 100) / totalBytes);
  Serial.println("%)");

  Serial.print("    Free:  ");
  Serial.print((uint32_t)(freeBytes / (1024 * 1024)));
  Serial.print(" MB (");
  Serial.print((freeBytes * 100) / totalBytes);
  Serial.println("%)");
}

void countSpace(File dir, uint64_t &total) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    if (entry.isDirectory()) {
      countSpace(entry, total);
    } else {
      total += entry.size();
    }
    entry.close();
  }
}

void listFiles(const char* dirname, bool recursive) {
  File root = SD.open(dirname);
  if (!root) {
    Serial.println("  ❌ Failed to open directory");
    return;
  }

  if (!root.isDirectory()) {
    Serial.println("  ❌ Not a directory");
    return;
  }

  int fileCount = 0;
  int dirCount = 0;
  uint64_t totalSize = 0;

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;

    Serial.print("  ");

    if (entry.isDirectory()) {
      Serial.print("[DIR]  ");
      Serial.println(entry.name());
      dirCount++;

      if (recursive && strcmp(entry.name(), ".") != 0 && strcmp(entry.name(), "..") != 0) {
        // Don't recurse for now - keeps output clean
      }
    } else {
      Serial.print("[FILE] ");

      // Filename
      String name = String(entry.name());
      Serial.print(name);

      // Padding
      for (int i = name.length(); i < 30; i++) {
        Serial.print(" ");
      }

      // Size
      uint32_t size = entry.size();
      Serial.print(size);
      Serial.print(" bytes");

      // Check file type
      name.toLowerCase();
      if (name.endsWith(".mp3")) {
        Serial.print("  ✓ MP3");
      } else if (name.endsWith(".wav")) {
        Serial.print("  ✓ WAV");
      } else if (name.endsWith(".txt")) {
        Serial.print("  ✓ TXT");
      }

      Serial.println();

      fileCount++;
      totalSize += size;
    }

    entry.close();
  }
  root.close();

  Serial.println();
  Serial.print("  Total: ");
  Serial.print(fileCount);
  Serial.print(" files, ");
  Serial.print(dirCount);
  Serial.print(" directories, ");
  Serial.print((uint32_t)(totalSize / 1024));
  Serial.println(" KB");
}

void readTest() {
  Serial.println("╔═══ READ TEST ═══╗");

  // Find first file
  File root = SD.open("/");
  if (!root) {
    Serial.println("  ❌ Cannot open root");
    return;
  }

  File testFile;
  String fileName;

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      fileName = String(entry.name());
      entry.close();
      break;
    }
    entry.close();
  }
  root.close();

  if (fileName.length() == 0) {
    Serial.println("  ❌ No files found to test");
    return;
  }

  Serial.print("  Testing file: ");
  Serial.println(fileName);

  testFile = SD.open(fileName.c_str(), FILE_READ);
  if (!testFile) {
    Serial.println("  ❌ Cannot open file");
    return;
  }

  uint32_t fileSize = testFile.size();
  Serial.print("  File size: ");
  Serial.print(fileSize);
  Serial.println(" bytes");

  // Read first 512 bytes
  uint8_t buffer[512];
  uint32_t toRead = min(fileSize, (uint32_t)512);

  Serial.print("  Reading first ");
  Serial.print(toRead);
  Serial.println(" bytes...");

  unsigned long startTime = micros();
  size_t bytesRead = testFile.read(buffer, toRead);
  unsigned long elapsed = micros() - startTime;

  Serial.print("  Read ");
  Serial.print(bytesRead);
  Serial.print(" bytes in ");
  Serial.print(elapsed);
  Serial.println(" μs");

  if (bytesRead > 0) {
    Serial.println("\n  First 128 bytes (hex):");
    for (size_t i = 0; i < min(bytesRead, (size_t)128); i++) {
      if (i % 16 == 0) {
        Serial.print("  ");
        if (i < 100) Serial.print(" ");
        if (i < 10) Serial.print(" ");
        Serial.print(i);
        Serial.print(": ");
      }

      if (buffer[i] < 16) Serial.print("0");
      Serial.print(buffer[i], HEX);
      Serial.print(" ");

      if ((i + 1) % 16 == 0) {
        Serial.println();
      }
    }
    Serial.println();

    // Check for common file signatures
    Serial.println("  File signature check:");
    if (bytesRead >= 3 && buffer[0] == 'I' && buffer[1] == 'D' && buffer[2] == '3') {
      Serial.println("    ✓ ID3 tag (MP3 with metadata)");
    } else if (bytesRead >= 2 && buffer[0] == 0xFF && (buffer[1] & 0xE0) == 0xE0) {
      Serial.println("    ✓ MP3 frame sync");
    } else if (bytesRead >= 4 && buffer[0] == 'R' && buffer[1] == 'I' && buffer[2] == 'F' && buffer[3] == 'F') {
      Serial.println("    ✓ RIFF header (WAV file)");
    } else {
      Serial.println("    ? Unknown format");
    }
  }

  testFile.close();
  Serial.println("\n  ✓ Read test complete");
}

void writeTest() {
  Serial.println("╔═══ WRITE TEST ═══╗");
  Serial.println("  Creating test.txt...");

  File testFile = SD.open("test.txt", FILE_WRITE);
  if (!testFile) {
    Serial.println("  ❌ Cannot create file");
    return;
  }

  String testData = "SD Card Write Test\n";
  testData += "Timestamp: ";
  testData += String(millis());
  testData += " ms\n";
  testData += "This is a test file.\n";

  size_t written = testFile.print(testData);
  testFile.close();

  Serial.print("  Written ");
  Serial.print(written);
  Serial.println(" bytes");

  // Verify
  testFile = SD.open("test.txt", FILE_READ);
  if (!testFile) {
    Serial.println("  ❌ Cannot open file for verification");
    return;
  }

  Serial.println("\n  Verification - file content:");
  while (testFile.available()) {
    Serial.write(testFile.read());
  }
  testFile.close();

  Serial.println("\n  ✓ Write test complete");
}

void deleteTest() {
  Serial.println("╔═══ DELETE TEST ═══╗");

  if (!SD.exists("test.txt")) {
    Serial.println("  ℹ File test.txt does not exist");
    Serial.println("  Run write test first (type 'w')");
    return;
  }

  Serial.print("  Deleting test.txt... ");

  if (SD.remove("test.txt")) {
    Serial.println("✓ OK");
  } else {
    Serial.println("❌ Failed");
  }
}

void formatInfo() {
  Serial.println("╔═══ FORMAT INFO ═══╗");
  Serial.println("  This tool cannot detect filesystem type directly,");
  Serial.println("  but you can infer it from card size:");
  Serial.println();

  uint64_t cardSize = SD.size();
  uint32_t cardSizeMB = cardSize / (1024 * 1024);

  Serial.print("  Card size: ");
  Serial.print(cardSizeMB);
  Serial.println(" MB");

  Serial.println("\n  Recommended formats:");
  if (cardSizeMB <= 2048) {
    Serial.println("    ✓ FAT16 or FAT32");
  } else if (cardSizeMB <= 32768) {
    Serial.println("    ✓ FAT32 (recommended)");
  } else {
    Serial.println("    ✓ exFAT (for >32GB)");
    Serial.println("    Note: arduino-pico SD library works best with FAT32");
  }

  Serial.println("\n  If experiencing issues, format as FAT32:");
  Serial.println("    - Windows: Right-click drive -> Format -> FAT32");
  Serial.println("    - Mac: Disk Utility -> Erase -> MS-DOS (FAT)");
  Serial.println("    - Linux: sudo mkfs.vfat -F 32 /dev/sdX");
}

void speedTest() {
  Serial.println("╔═══ SPEED TEST ═══╗");

  // Write test
  Serial.println("  Write Speed Test:");
  Serial.print("    Creating 100KB file... ");

  File testFile = SD.open("speedtest.bin", FILE_WRITE);
  if (!testFile) {
    Serial.println("❌ Failed");
    return;
  }

  uint8_t buffer[512];
  for (int i = 0; i < 512; i++) {
    buffer[i] = i & 0xFF;
  }

  unsigned long startTime = millis();

  for (int i = 0; i < 200; i++) { // 200 * 512 = 100KB
    testFile.write(buffer, 512);
  }

  unsigned long elapsed = millis() - startTime;
  testFile.close();

  float speedKBs = (100.0 * 1000.0) / elapsed;

  Serial.print("✓ ");
  Serial.print(speedKBs);
  Serial.println(" KB/s");

  // Read test
  Serial.println("  Read Speed Test:");
  Serial.print("    Reading 100KB file... ");

  testFile = SD.open("speedtest.bin", FILE_READ);
  if (!testFile) {
    Serial.println("❌ Failed");
    return;
  }

  startTime = millis();

  while (testFile.available()) {
    testFile.read(buffer, 512);
  }

  elapsed = millis() - startTime;
  testFile.close();

  speedKBs = (100.0 * 1000.0) / elapsed;

  Serial.print("✓ ");
  Serial.print(speedKBs);
  Serial.println(" KB/s");

  // Cleanup
  SD.remove("speedtest.bin");

  Serial.println("\n  Speed classification:");
  if (speedKBs > 1000) {
    Serial.println("    ✓ Excellent (>1 MB/s)");
  } else if (speedKBs > 500) {
    Serial.println("    ✓ Good (>500 KB/s)");
  } else if (speedKBs > 200) {
    Serial.println("    ⚠ Moderate (>200 KB/s)");
  } else {
    Serial.println("    ❌ Slow (<200 KB/s) - Check card quality");
  }
}
