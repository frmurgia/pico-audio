// SD Card Write Test Example
// Tests SD card write functionality
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

  Serial.println("\n=== SD Card Write Test ===");
  Serial.println("Testing SD card write functionality");

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
    Serial.println("  - Card is not write-protected");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("SD card initialized successfully!");
  Serial.println();

  // Run write tests
  testFileWrite();
  testFileAppend();
  testBinaryWrite();
  testDirectoryCreate();

  Serial.println("\n=== All Write Tests Complete ===");
}

void loop() {
  // Update a log file every 5 seconds
  static unsigned long lastLog = 0;

  if (millis() - lastLog > 5000) {
    lastLog = millis();

    File logFile = SD.open("/datalog.txt", FILE_WRITE);
    if (logFile) {
      // Write timestamp and random data
      logFile.print(millis());
      logFile.print(",");
      logFile.print(analogRead(A0));
      logFile.print(",");
      logFile.println(random(0, 100));
      logFile.close();

      Serial.println("Data logged to /datalog.txt");
    } else {
      Serial.println("ERROR: Could not open log file");
    }
  }
}

void testFileWrite() {
  Serial.println("--- Test 1: Create and Write File ---");

  // Create new file (overwrites if exists)
  File testFile = SD.open("/test.txt", FILE_WRITE);

  if (testFile) {
    Serial.println("Writing to /test.txt...");

    testFile.println("SD Card Write Test");
    testFile.println("==================");
    testFile.println();
    testFile.println("This file was created by pico-audio SD Write Test");
    testFile.print("Timestamp: ");
    testFile.println(millis());
    testFile.println();
    testFile.println("If you can read this, SD write is working!");

    testFile.close();
    Serial.println("File written successfully!");

    // Verify by reading back
    testFile = SD.open("/test.txt");
    if (testFile) {
      Serial.println("\nVerifying file contents:");
      Serial.println("------------------------");
      while (testFile.available()) {
        Serial.write(testFile.read());
      }
      Serial.println("------------------------");
      testFile.close();
    }
  } else {
    Serial.println("ERROR: Could not create file!");
  }

  Serial.println();
}

void testFileAppend() {
  Serial.println("--- Test 2: Append to File ---");

  File testFile = SD.open("/test.txt", FILE_WRITE);

  if (testFile) {
    // Seek to end of file
    testFile.seek(testFile.size());

    Serial.println("Appending to /test.txt...");
    testFile.println();
    testFile.println("--- Appended Data ---");
    testFile.print("Append timestamp: ");
    testFile.println(millis());

    testFile.close();
    Serial.println("Data appended successfully!");
  } else {
    Serial.println("ERROR: Could not open file for append!");
  }

  Serial.println();
}

void testBinaryWrite() {
  Serial.println("--- Test 3: Binary Write ---");

  File binFile = SD.open("/binary.dat", FILE_WRITE);

  if (binFile) {
    Serial.println("Writing binary data to /binary.dat...");

    // Write some binary data
    uint8_t data[256];
    for (int i = 0; i < 256; i++) {
      data[i] = i;
    }

    size_t written = binFile.write(data, 256);
    binFile.close();

    Serial.print("Wrote ");
    Serial.print(written);
    Serial.println(" bytes");

    // Verify
    binFile = SD.open("/binary.dat");
    if (binFile) {
      Serial.println("Verifying binary data...");
      bool verified = true;
      for (int i = 0; i < 256; i++) {
        uint8_t readByte = binFile.read();
        if (readByte != i) {
          verified = false;
          Serial.print("ERROR at byte ");
          Serial.print(i);
          Serial.print(": expected ");
          Serial.print(i);
          Serial.print(", got ");
          Serial.println(readByte);
          break;
        }
      }
      binFile.close();

      if (verified) {
        Serial.println("Binary data verified OK!");
      }
    }
  } else {
    Serial.println("ERROR: Could not create binary file!");
  }

  Serial.println();
}

void testDirectoryCreate() {
  Serial.println("--- Test 4: Create Directory ---");

  if (SD.mkdir("/testdir")) {
    Serial.println("Directory /testdir created successfully!");

    // Create file in directory
    File dirFile = SD.open("/testdir/info.txt", FILE_WRITE);
    if (dirFile) {
      dirFile.println("This file is in a subdirectory!");
      dirFile.close();
      Serial.println("Created /testdir/info.txt");
    }
  } else {
    Serial.println("Directory already exists or could not be created");
  }

  Serial.println();
}
