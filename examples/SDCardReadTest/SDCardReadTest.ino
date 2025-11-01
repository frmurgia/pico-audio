// SD Card Read Test Example
// Tests SD card connectivity and lists all files
//
// SD Card Wiring (SPI):
// - SCK (Clock)  -> GP6
// - MOSI (Data In) -> GP7
// - MISO (Data Out) -> GP4
// - CS (Chip Select) -> GP5

#include <Adafruit_TinyUSB.h>
#include <SPI.h>
#include <SD.h>

// SD Card pin configuration
#define SD_CS_PIN   5    // Chip Select
#define SD_SCK_PIN  6    // Clock
#define SD_MOSI_PIN 7    // Master Out Slave In
#define SD_MISO_PIN 4    // Master In Slave Out

void setup() {
  Serial.begin(115200);

  // Wait for serial connection (max 5 seconds)
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime < 5000)) {
    delay(100);
  }

  Serial.println("\n=== SD Card Read Test ===");
  Serial.println("Testing SD card connectivity and file listing");

  // Configure SPI pins
  SPI.setRX(SD_MISO_PIN);
  SPI.setTX(SD_MOSI_PIN);
  SPI.setSCK(SD_SCK_PIN);

  // Initialize SD card
  Serial.print("Initializing SD card on CS pin ");
  Serial.print(SD_CS_PIN);
  Serial.println("...");

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("ERROR: SD card initialization failed!");
    Serial.println("Check:");
    Serial.println("  - SD card is inserted");
    Serial.println("  - Wiring connections");
    Serial.println("  - Card is formatted (FAT16/FAT32)");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("SD card initialized successfully!");
  Serial.println();

  // Print card info
  printCardInfo();

  // List all files
  Serial.println("\n=== Root Directory Files ===");
  File root = SD.open("/");
  listDirectory(root, 0);
  root.close();

  Serial.println("\n=== Test Complete ===");
  Serial.println("SD card is working correctly!");
}

void loop() {
  // Read test every 10 seconds
  static unsigned long lastRead = 0;

  if (millis() - lastRead > 10000) {
    lastRead = millis();

    Serial.println("\n--- Periodic Read Test ---");

    // Try to read a test file
    File testFile = SD.open("/test.txt");
    if (testFile) {
      Serial.println("Reading /test.txt:");
      while (testFile.available()) {
        Serial.write(testFile.read());
      }
      testFile.close();
      Serial.println("\nFile read successfully!");
    } else {
      Serial.println("File /test.txt not found");
      Serial.println("You can create it using the SD Write Test example");
    }
  }
}

void printCardInfo() {
  uint8_t cardType = SD.type();

  Serial.print("Card Type: ");
  switch (cardType) {
    case CARD_NONE:
      Serial.println("No SD card");
      break;
    case CARD_MMC:
      Serial.println("MMC");
      break;
    case CARD_SD:
      Serial.println("SDSC");
      break;
    case CARD_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknown");
  }

  uint64_t cardSize = SD.size64() / (1024 * 1024);
  Serial.print("Card Size: ");
  Serial.print(cardSize);
  Serial.println(" MB");

  uint64_t usedSize = SD.usedSize64() / (1024 * 1024);
  Serial.print("Used Space: ");
  Serial.print(usedSize);
  Serial.println(" MB");

  uint64_t freeSize = cardSize - usedSize;
  Serial.print("Free Space: ");
  Serial.print(freeSize);
  Serial.println(" MB");
}

void listDirectory(File dir, int numTabs) {
  int fileCount = 0;
  int dirCount = 0;

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      break;
    }

    // Print indentation
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print("  ");
    }

    // Print filename
    Serial.print(entry.name());

    if (entry.isDirectory()) {
      Serial.println("/");
      dirCount++;

      // Recursively list subdirectory (max depth 3)
      if (numTabs < 3) {
        listDirectory(entry, numTabs + 1);
      }
    } else {
      // Print file size
      Serial.print(" - ");
      Serial.print(entry.size());
      Serial.println(" bytes");
      fileCount++;
    }

    entry.close();
  }

  // Print summary at root level
  if (numTabs == 0) {
    Serial.println();
    Serial.print("Total: ");
    Serial.print(fileCount);
    Serial.print(" files, ");
    Serial.print(dirCount);
    Serial.println(" directories");
  }
}
