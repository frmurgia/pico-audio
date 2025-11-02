# Flash WAV Player - LittleFS Version

Riproduce file WAV dalla **flash memory interna** invece che dalla SD card!

## 🎯 Vantaggi rispetto SD Card

| Caratteristica | Flash (LittleFS) | SD SPI | SD SDIO |
|----------------|------------------|--------|---------|
| **Velocità** | ~3-4 MB/s | ~1-2 MB/s | ~10-12 MB/s |
| **Capacità** | ~14 MB | Illimitato | Illimitato |
| **Wiring** | Nessuno! | 4 pin | 6 pin |
| **Affidabilità** | ✓✓✓ Alta | ✓ Media | ✓✓ Alta |
| **Costo** | Gratis | €2-5 (card) | €5-10 (card) |

**Ideale per:** 10-15 file WAV pre-caricati, demo, prodotti finali senza SD card

## 📦 Capacità

- **Flash totale:** 16 MB
- **Codice:** ~2 MB
- **Filesystem (LittleFS):** ~14 MB disponibili
- **File WAV:** Circa 10-15 file da 1MB ciascuno

## 🚀 Come caricare i WAV nella Flash

### Metodo 1: Arduino IDE LittleFS Upload (Consigliato)

1. **Installa il tool:**
   - Scarica: https://github.com/earlephilhower/arduino-pico-littlefs-plugin/releases
   - Estrai in `~/Documents/Arduino/tools/`

2. **Crea cartella dati:**
   ```bash
   cd FlashWavPlayer
   mkdir data
   ```

3. **Copia i WAV:**
   ```bash
   cp track1.wav track2.wav ... track10.wav data/
   ```

4. **Upload filesystem:**
   - Arduino IDE → Tools → Pico LittleFS Data Upload
   - Aspetta il completamento (~30 secondi)

5. **Upload sketch:**
   - Normale upload del codice

### Metodo 2: PlatformIO

**platformio.ini:**
```ini
[env:pimoroni_pico_plus_2]
platform = raspberrypi
board = rpipico2
framework = arduino

board_build.filesystem_size = 14m  ; 14MB per LittleFS

extra_scripts = pre:upload_fs.py
```

**Comandi:**
```bash
# 1. Crea cartella dati e copia WAV
mkdir data
cp *.wav data/

# 2. Upload filesystem
pio run --target uploadfs

# 3. Upload codice
pio run --target upload
```

### Metodo 3: Tool Python (Avanzato)

```python
# upload_littlefs.py
import serial
import time

ser = serial.Serial('/dev/ttyACM0', 115200)

# Formatta LittleFS
ser.write(b'FORMAT\n')
time.sleep(5)

# Upload file
with open('track1.wav', 'rb') as f:
    data = f.read()
    ser.write(f'UPLOAD /track1.wav {len(data)}\n'.encode())
    ser.write(data)

ser.close()
```

## 📋 Formato File

I file WAV devono essere nominati:
```
/track1.wav
/track2.wav
/track3.wav
...
/track10.wav
```

**Formato audio:**
- Sample rate: 44.1 kHz
- Bit depth: 16-bit
- Channels: Mono o Stereo (convertito automaticamente a mono)

## 🎮 Comandi Serial

- `'1'-'9','0'` → Play/stop track 1-10
- `'s'` → Stop all
- `'l'` → Mostra file in flash
- `'d'` → Debug info
- `'v'/'b'` → Volume su/giù

## ⚡ Performance

**Con LittleFS (Flash):**
- 10 file @ 44.1kHz stereo = 1.76 MB/s richiesti
- Flash fornisce ~3-4 MB/s = **2x headroom**
- Buffer 16KB = 372ms per player
- ✓ Zero underruns
- ✓ CPU <5%

## 🔧 Troubleshooting

### "LittleFS mount failed"

La flash non è formattata. Opzioni:

**Opzione A - Tool automatico:**
```cpp
// In setup(), prima di LittleFS.begin()
LittleFS.format();  // FORMATTA (cancella tutto!)
```

**Opzione B - Arduino IDE:**
1. Tools → Erase Flash → All Flash Contents
2. Upload sketch
3. Upload filesystem data

### "File not found"

1. Verifica nomi file:
   ```bash
   # Comandi seriali
   > l
   ===== FILES IN FLASH =====
     /track1.wav (1234 KB)
     /track2.wav (987 KB)
   ```

2. Verifica path con `/`:
   ```cpp
   snprintf(filename, "/track1.wav");  // ✓ Corretto
   snprintf(filename, "track1.wav");   // ✗ Sbagliato
   ```

### "Out of space"

```bash
# Verifica spazio
> l
Total: 15 files, 15234 KB / 14336 KB available
Used: 106%  ← TROPPO!
```

Soluzioni:
- Riduci numero file
- Comprimi WAV (sample rate più basso)
- Aumenta filesystem_size in platformio.ini

## 📊 Comparazione completa

### Flash (LittleFS) - Questa versione
✓ Veloce (3-4 MB/s)
✓ No hardware extra
✓ Affidabile
⚠️ Solo 14MB (~15 file)
⚠️ Devi ricaricare per cambiare file

### SD SPI
✓ Capacità illimitata
✓ Facile cambiare file
⚠️ Lento (1-2 MB/s)
⚠️ Serve SD card
⚠️ 4 pin wiring

### SD SDIO
✓ Veloce (10-12 MB/s)
✓ Capacità illimitata
⚠️ 6 pin wiring
⚠️ Non tutte le SD supportano SDIO
⚠️ Serve SD card costosa

### PROGMEM (array in codice)
✓ Velocissimo (20+ MB/s)
⚠️ Binario enorme
⚠️ Solo 16MB max
⚠️ Devi ricompilare per ogni modifica
❌ I tuoi 98MB NON entrano!

## 🎯 Conclusione

**Usa LittleFS quando:**
- Hai 10-15 file fissi
- Vuoi massima affidabilità
- Non vuoi SD card
- Stai facendo un prodotto commerciale

**Usa SD quando:**
- Hai molti file (>15)
- File cambiano spesso
- Hai >14MB di audio

**NON usare PROGMEM quando:**
- Hai più di pochi file piccoli
- File potrebbero cambiare
- Vuoi flessibilità
