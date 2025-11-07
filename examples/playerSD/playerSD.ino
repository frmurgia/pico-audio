#include <Adafruit_TinyUSB.h>
#include <SD.h>
#include <pico-audio.h> // Assumendo che il tuo play_sd_wav.h sia qui

// SDIO Pin Configuration
#define SD_CLK_PIN  6
#define SD_CMD_PIN  7
#define SD_DAT0_PIN 8

// I2S Pin Configuration
#define I2S_BCLK_PIN 20
#define I2S_LRCLK_PIN 21
#define I2S_DIN_PIN 22

// Oggetti Audio Minimi
AudioPlaySdWav player1;     // UN SOLO player
AudioOutputI2S i2s1;        // UN SOLO output

// Connessioni dirette
AudioConnection patchCord1(player1, 0, i2s1, 0); // Canale Sinistro
AudioConnection patchCord2(player1, 1, i2s1, 1); // Canale Destro

void setup() {
  Serial.begin(115200);
  delay(3000); // Attesa per il monitor seriale
  Serial.println("\n--- Test Minimo AudioPlaySdWav ---");

  // 1. Inizializza l'audio
  AudioMemory(20); // Diamo un po' di memoria
  
  i2s1.begin();
  
  Serial.println("OK.");

  // 3. Inizializza la Scheda SD
  Serial.print("Inizializzo SD card (SDIO)... ");
  if (!SD.begin(SD_CLK_PIN, SD_CMD_PIN, SD_DAT0_PIN)) {
    Serial.println("FALLITO. Controlla i pin SDIO o la scheda.");
    while(1);
  }
  Serial.println("OK.");

  // 4. Prova a suonare il file
  Serial.print("Cerco 'track1.wav'... ");
  if (player1.play("track1.wav")) {
    Serial.println("✓ Trovato e in riproduzione!");
    Serial.println("Dovresti sentire l'audio ora.");
  } else {
    Serial.println("❌ FALLITO. File non trovato o illeggibile.");
  }
}

void loop() {
  // Stampa le statistiche per vedere se il player si ferma
  static unsigned long lastStats = 0;
  if (millis() - lastStats > 1000) {
    lastStats = millis();
    
    if (player1.isPlaying()) {
      Serial.print("♪ Suonando... Posizione: ");
      Serial.print(player1.positionMillis());
      Serial.println(" ms");
    } else {
      Serial.println("...Player fermo.");
    }
  }
}