#include <pico-audio.h>

#include <Adafruit_TinyUSB.h>
#include <SD.h>
// I TUOI PIN I2S - CAMBIA L'ORDINE SE NECESSARIO
#define I2S_BCLK_PIN 20
#define I2S_LRCLK_PIN 21
#define I2S_DIN_PIN 22

// Oggetti audio minimi
    // Un generatore di tono
AudioOutputI2S   i2s1;      // La nostra uscita hardware
AudioSynthWaveform sine1;
// Collega il generatore di tono direttamente all'uscita I2S
// Uscita 0 della sinusoide -> Canale 0 (Sinistro) di I2S
AudioConnection  patchCord1(sine1, 0, i2s1, 0);
// Uscita 0 della sinusoide -> Canale 1 (Destro) di I2S
AudioConnection  patchCord2(sine1, 0, i2s1, 1);

void setup() {
  Serial.begin(115200);
  Serial.println("Inizio Test Sinusoide I2S...");
   i2s1.begin();
    sine1.begin(WAVEFORM_BANDLIMIT_SAWTOOTH);
  // Alloca memoria per i blocchi audio
  AudioMemory(12);


  
  // Attiva la sinusoide
  // Frequenza (Hz), Ampiezza (0.0 - 1.0)
  sine1.frequency(440); 
  sine1.amplitude(0.5); 

  Serial.println("Test in corso. Dovresti sentire un tono di 440Hz.");
}

void loop() {
  // Lascia vuoto, la libreria audio funziona con gli interrupt
  delay(10);
}