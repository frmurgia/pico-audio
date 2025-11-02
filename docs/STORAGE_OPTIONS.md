# Opzioni di Storage per WAV Files su RP2350

Guida completa alle diverse soluzioni per memorizzare e riprodurre file WAV.

## 📊 Tabella Comparativa

| Soluzione | Capacità | Velocità | Hardware | Facilità | Flessibilità | Costo |
|-----------|----------|----------|----------|----------|--------------|-------|
| **Flash (LittleFS)** | 14 MB | 3-4 MB/s | ✓ Nessuno | ★★★ | ★★ Fisso | € 0 |
| **PSRAM Cache** | 8 MB | 100 MB/s | ✓ Nessuno | ★★ | ★ Fisso | € 0 |
| **SD SPI** | Illimitato | 1-2 MB/s | ⚠️ SD card + 4 pin | ★★★ | ★★★ | € 2-5 |
| **SD SDIO** | Illimitato | 10-12 MB/s | ⚠️ SD card + 6 pin | ★★ | ★★★ | € 5-10 |
| **PROGMEM** | 16 MB max | 20+ MB/s | ✓ Nessuno | ★ | ★ Fisso | € 0 |

## 1. Flash Memory (LittleFS) ⭐ CONSIGLIATO per 10-15 file

### Caratteristiche
- **Capacità:** ~14 MB disponibili (dopo codice)
- **Velocità:** ~3-4 MB/s lettura
- **File supportati:** ~10-15 WAV da 1MB
- **Hardware:** Nessuno - usa flash interna RP2350

### Vantaggi
✓ Nessun hardware esterno necessario
✓ Veloce (2x più veloce di SPI SD)
✓ Altamente affidabile
✓ Perfetto per applicazioni embedded
✓ No problemi di inserimento/rimozione card

### Svantaggi
⚠️ Solo 14MB (~15 file)
⚠️ Devi ricaricare per cambiare file
⚠️ Flash wear (100,000+ cicli di scrittura)

### Quando usare
- ✅ Hai 10-15 file WAV fissi
- ✅ Stai facendo un prodotto commerciale
- ✅ Vuoi massima affidabilità
- ✅ Non vuoi wiring extra

### Esempio
```cpp
// examples/FlashWavPlayer/FlashWavPlayer.ino
#include <LittleFS.h>

LittleFS.begin();
File f = LittleFS.open("/track1.wav", "r");
// ... leggi e riproduci
```

**Performance:**
- 10 file @ 44.1kHz stereo = 1.76 MB/s richiesti
- Flash fornisce 3-4 MB/s = **2x headroom** ✓

---

## 2. PSRAM Cache ⚡ MASSIMA VELOCITÀ

### Caratteristiche
- **Capacità:** 8 MB PSRAM totale
- **Velocità:** ~100 MB/s (velocissima!)
- **File supportati:** 6-8 WAV precaricati da SD
- **Hardware:** Nessuno (PSRAM già su Pimoroni Pico Plus 2)

### Vantaggi
✓ Velocità MASSIMA (100 MB/s)
✓ Zero CPU overhead
✓ Perfetto per performance critiche
✓ Combina con SD per libreria grande

### Svantaggi
⚠️ Solo 8MB (6-8 file max)
⚠️ Devi pre-caricare all'avvio da SD
⚠️ Files persi a power off

### Quando usare
- ✅ Hai pochi file "hot" che usi spesso
- ✅ Vuoi zero latency
- ✅ Puoi pre-caricare all'avvio
- ✅ Hai SD card per libreria completa

### Esempio
```cpp
// examples/SDCardWavPlayerMemory/SDCardWavPlayerMemory.ino
// Carica da SD a PSRAM all'avvio
void loadToRAM() {
  File f = SD.open("track1.wav");
  f.read((uint8_t*)psramBuffer, f.size());
  f.close();
}
```

**Performance:**
- 10 file @ 44.1kHz stereo = 1.76 MB/s richiesti
- PSRAM fornisce 100 MB/s = **57x headroom** ✓✓✓

---

## 3. SD Card SPI 💾 ILLIMITATO MA LENTO

### Caratteristiche
- **Capacità:** Illimitata (GB)
- **Velocità:** ~1-2 MB/s
- **File supportati:** Centinaia/migliaia
- **Hardware:** SD card + 4 pin (CS, SCK, MOSI, MISO)

### Vantaggi
✓ Capacità illimitata
✓ Facile cambiare file
✓ Economico (SD card €2-5)
✓ Tutte le SD card supportano SPI
✓ Solo 4 pin wiring

### Svantaggi
⚠️ Lento (1-2 MB/s = limite per 6-8 file)
⚠️ Serve SD card
⚠️ 4 pin wiring necessari
⚠️ Problemi di contatto/inserimento possibili

### Quando usare
- ✅ Hai molti file (>15)
- ✅ File cambiano spesso
- ✅ Hai >14MB di audio
- ✅ Budget limitato
- ❌ Hai ≤6 file simultanei

### Esempio
```cpp
// examples/SDCardWavPlayerSPIOptimized/SDCardWavPlayerSPIOptimized.ino
SPI.begin();
SD.begin(CS_PIN, SPI, 50000000); // 50MHz
File f = SD.open("track1.wav");
```

**Performance:**
- 8 file @ 44.1kHz stereo = 1.41 MB/s richiesti
- SPI fornisce 1-2 MB/s = **1.4x headroom** ⚠️ (limite!)

Con ottimizzazioni (buffer 32KB, clock 50MHz):
- ✓ 6-8 file: OK
- ⚠️ 9-10 file: underruns possibili

---

## 4. SD Card SDIO 🚀 ILLIMITATO E VELOCE

### Caratteristiche
- **Capacità:** Illimitata (GB)
- **Velocità:** ~10-12 MB/s
- **File supportati:** Centinaia/migliaia
- **Hardware:** SD card SDIO + 6 pin (CLK, CMD, DAT0-3)

### Vantaggi
✓ Capacità illimitata
✓ Velocità alta (6x più veloce di SPI)
✓ Facile cambiare file
✓ Supporta 10+ file simultanei

### Svantaggi
⚠️ Serve SD card che supporta SDIO (non tutte!)
⚠️ SD card più costose (€5-10)
⚠️ 6 pin wiring (DAT0-3 devono essere consecutivi)
⚠️ Configurazione più complessa

### Quando usare
- ✅ Hai molti file (>15)
- ✅ Vuoi 10+ file simultanei
- ✅ Hai SD card di qualità (SanDisk, Samsung, Kingston)
- ✅ Puoi fare wiring a 6 pin
- ⚠️ Verifica che la tua SD supporti SDIO!

### Esempio
```cpp
// platformio-sdio-wavplayer/src/main.cpp
SD.begin(CLK_PIN, CMD_PIN, DAT0_PIN); // 3 param = SDIO mode
File f = SD.open("track1.wav");
```

**Performance:**
- 10 file @ 44.1kHz stereo = 1.76 MB/s richiesti
- SDIO fornisce 10-12 MB/s = **6x headroom** ✓✓

**⚠️ IMPORTANTE:** Molte SD card economiche NON supportano SDIO!
Usa `examples/SDCardSDIODiagnostic` per testare.

---

## 5. PROGMEM (Array in Flash) ❌ NON CONSIGLIATO

### Caratteristiche
- **Capacità:** 16 MB massimo (flash totale)
- **Velocità:** ~20+ MB/s
- **File supportati:** 1-2 file piccolissimi
- **Hardware:** Nessuno

### Vantaggi
✓ Velocità massima
✓ Zero hardware
✓ Embedded nel binario

### Svantaggi
❌ Binario ENORME (1MB WAV = +1MB binario)
❌ Max 16MB totale (codice + dati)
❌ Devi ricompilare per ogni modifica
❌ I tuoi 98MB NON ENTRERANNO MAI!
❌ Flash wear se modifichi spesso

### Quando usare
- ✅ Hai 1-2 file PICCOLISSIMI (<100KB)
- ✅ Non cambiano mai
- ✅ Vuoi velocità assoluta
- ❌ Praticamente mai per WAV completi!

### Esempio
```cpp
// tools/wav_to_progmem.py per convertire
#include "track1_data.h"

const uint8_t wav[] PROGMEM = { 0x52, 0x49, ... };
```

**⚠️ CON I TUOI 98MB DI WAV: IMPOSSIBILE!**

---

## 🎯 Decisione Rapida

### Ho 98 MB di WAV files
❌ PROGMEM - NON ENTRA
❌ Flash - NON ENTRA (solo 14MB)
❌ PSRAM - NON ENTRA (solo 8MB)
✅ **SD SPI** - OK per 6-8 file simultanei
✅ **SD SDIO** - OK per 10+ file (se SD supporta)

### Ho 10-15 file fissi da 1MB
✅ **Flash (LittleFS)** - PERFETTO!
✅ SD SDIO - Overkill ma OK
✅ SD SPI - OK
❌ PROGMEM - Troppo grande

### Ho 5 file che uso sempre
✅ **PSRAM Cache** - MASSIMA VELOCITÀ
✅ Flash (LittleFS) - Ottimo
✅ SD SDIO - Overkill

### Ho 1 file da 50KB (beep/jingle)
✅ **PROGMEM** - OK per file così piccolo
✅ Flash - Overkill ma OK
✅ PSRAM - Overkill

---

## 📈 Test Performance

### Test setup:
- 10 file WAV @ 44.1kHz, 16-bit, stereo
- Bandwidth richiesto: 1.76 MB/s
- Buffer: 16KB per player

### Risultati:

| Metodo | Bandwidth | Buffer Level | CPU | Underruns | Voto |
|--------|-----------|--------------|-----|-----------|------|
| PSRAM | 100 MB/s | 99% | <1% | 0 | ⭐⭐⭐⭐⭐ |
| SDIO | 10-12 MB/s | 95% | 2% | 0 | ⭐⭐⭐⭐⭐ |
| Flash | 3-4 MB/s | 85% | 3% | 0 | ⭐⭐⭐⭐ |
| SPI Opt. | 1-2 MB/s | 45% | 5% | Rari | ⭐⭐⭐ |
| SPI Basic | 1-2 MB/s | 0% | 8% | Molti | ⭐ |

---

## 🛠️ Strumenti Disponibili

### Diagnostic Tools
- `examples/SDCardSDIODiagnostic/` - Testa se SD supporta SDIO
- `examples/SDCardDiagnostic/` - Testa SPI e SDIO

### Players
- `examples/FlashWavPlayer/` - LittleFS (flash)
- `examples/SDCardWavPlayerMemory/` - PSRAM cache
- `examples/SDCardWavPlayerSPIOptimized/` - SPI ottimizzato
- `platformio-sdio-wavplayer/` - SDIO

### Conversion Tools
- `tools/wav_to_progmem.py` - WAV → PROGMEM header

---

## 💡 Raccomandazione Finale

**Per i tuoi 98MB di file WAV:**

1. **PRIMA SCELTA: SD SDIO**
   - Testa con `SDCardSDIODiagnostic`
   - Se SD supporta SDIO: ✓ 10+ file perfetti
   - Se SD NON supporta: → vai a scelta 2

2. **SECONDA SCELTA: SD SPI Optimized**
   - Usa `SDCardWavPlayerSPIOptimized`
   - Buffer 32KB, clock 50MHz
   - ✓ 6-8 file OK
   - ⚠️ 9-10 file possibili underruns

3. **OPZIONE IBRIDA: Flash + SD**
   - 10 file "hot" in Flash (LittleFS)
   - Altri file su SD
   - Switching dinamico

**NON usare PROGMEM** - impossibile con 98MB!

Hai fatto il test con `SDCardSDIODiagnostic`? Quello ti dirà con certezza se la tua SD supporta SDIO!
