# SD Card MP3 Player - SPI DEBUG VERSION

Versione **semplificata con SPI e debug completo** per verificare passo-passo il funzionamento del lettore MP3.

## 🎯 Scopo

Questa versione è pensata per **diagnosticare problemi** verificando:
1. ✅ Inizializzazione SD card
2. ✅ Lettura file dalla SD
3. ✅ Decodifica MP3
4. ✅ Output audio I2S
5. ✅ Formato file MP3

## 📋 Hardware

### Raspberry Pi Pico 2
Qualsiasi variante (RP2350)

### DAC I2S - PCM5102
```
PCM5102 -> Pico 2
BCK  -> GP20
LRCK -> GP21
DIN  -> GP22
VCC  -> 3.3V
GND  -> GND
```

### SD Card con SPI
```
SD Card -> Pico 2
CS   -> GP3
SCK  -> GP6
MOSI -> GP7
MISO -> GP0
VCC  -> 3.3V
GND  -> GND
```

**⚠️ NOTA**: Questi sono pin SPI standard, più facili da debuggare rispetto a SDIO.

## 📁 Preparazione SD Card

1. **Formatta in FAT32**
2. **Copia file MP3** nominati: `track1.mp3`, `track2.mp3`, etc.
3. **Verifica formato**:
   - Bitrate: qualsiasi (testato 128-320kbps)
   - Sample rate: preferibilmente 44.1kHz
   - Canali: mono o stereo (auto-convertito)

## 🚀 Test Step-by-Step

### 1. Upload lo Sketch
- Arduino IDE → Raspberry Pi Pico 2
- Upload `SDCardMP3PlayerSPI.ino`
- Apri Serial Monitor (115200 baud)

### 2. Verifica Output Iniziale

Dovresti vedere:

```
╔═══════════════════════════════════════════╗
║  SD MP3 Player - SPI DEBUG VERSION       ║
║  v1.0 - Diagnostic Mode                  ║
╚═══════════════════════════════════════════╝

╔═══ STEP 1: SPI INITIALIZATION ═══╗
  Configuring SPI... OK
  MISO: GP0
  MOSI: GP7
  SCK:  GP6
  CS:   GP3
```

**❌ Se non vedi questo**:
- Verifica connessione USB
- Verifica baudrate Serial Monitor = 115200

### 3. Verifica SD Card

```
╔═══ STEP 2: SD CARD INITIALIZATION ═══╗
  Initializing SD card... OK!
  ✓ Card size: 8192 MB
  ✓ Card type: SDHC (2-32GB)
```

**❌ Se vedi "FAILED"**:
1. Controlla wiring SPI (CS=GP3, SCK=GP6, MOSI=GP7, MISO=GP0)
2. Verifica SD card inserita correttamente
3. Prova altra SD card
4. Verifica 3.3V sulla SD card

### 4. Verifica File Listing

```
╔═══ STEP 3: FILE LISTING ═══╗
  Files on SD card:
    track1.mp3 (3458291 bytes) ✓ MP3
    track2.mp3 (2891234 bytes) ✓ MP3

  Total files: 2 (2 MP3 files)
```

**❌ Se vedi "No MP3 files found"**:
1. Verifica file sulla SD
2. Assicurati che siano nominati `track1.mp3`, `track2.mp3`, etc.
3. Verifica estensione `.mp3` (non `.MP3`)

### 5. Test Decoder MP3

Digita `t` nel Serial Monitor:

```
╔═══ MP3 DECODE TEST ═══╗
  Testing file: track1.mp3
  File size: 3458291 bytes
  Read 4096 bytes
  First bytes: 49 44 33 04 00 00 00 ...
  ✓ ID3 tag detected

  Attempting decode... SUCCESS!
    Samples: 1152
    Sample rate: 44100 Hz
    Channels: 2
    Bitrate: 320 kbps
    Frame size: 1044 bytes
    First samples: -245 128 542 -89 ...
```

**✅ Se vedi "SUCCESS"**: Il decoder funziona! I tuoi MP3 sono OK.

**❌ Se vedi "FAILED"**:
1. File MP3 potrebbe essere corrotto
2. Formato non standard
3. Prova con altro file MP3
4. Ricodifica MP3 con:
   ```bash
   ffmpeg -i input.mp3 -ar 44100 -b:a 192k output.mp3
   ```

### 6. Test Audio Output

Digita `1` per riprodurre track1.mp3:

```
▶ Playing: track1.mp3
  File size: 3458291 bytes
  ✓ Started

♪ Playing | Buf: 87% | Frames: 125 | Samples: 144000 | Pos: 131072/3458291 (3%)
♪ Playing | Buf: 91% | Frames: 251 | Samples: 288000 | Pos: 262144/3458291 (7%)
```

**✅ Se vedi stats aggiornate**: Tutto funziona! Dovresti sentire audio.

**❌ Se vedi solo "Started" ma niente stats**:
- Problema nel decoder o nella lettura
- Digita `d` per debug info

**❌ Se non senti audio**:
1. Verifica wiring DAC (BCK=GP20, LRCK=GP21, DIN=GP22)
2. Verifica alimentazione DAC (3.3V)
3. Verifica cuffie/speaker connessi
4. Verifica volume DAC (alcuni PCM5102 hanno jumper)

## 🔧 Comandi Debug

| Comando | Descrizione |
|---------|-------------|
| `1-9,0` | Play track 1-10 |
| `s` | Stop playback |
| `l` | List files again |
| `i` | SD card info (spazio, etc.) |
| `t` | Test MP3 decode (primo file) |
| `d` | Debug info dettagliato |

### Esempio Debug Info

Digita `d`:

```
╔═══ DEBUG INFO ═══╗
  Playing: YES
  File: track1.mp3
  Position: 524288 / 3458291 (15%)
  MP3 buffer: 2048 / 4096 bytes
  Audio buffer: 7123 / 8192 samples (86%)
  Frames decoded: 502
  Samples generated: 578304

  Audio Memory: 8 blocks
  CPU Usage: 15%
```

**Verifica**:
- Buffer % > 50% = OK
- Frames decoded aumenta = Decoder OK
- CPU Usage < 50% = Performance OK

## 🐛 Troubleshooting

### Problema: SD card FAILED

**Verifica wiring**:
```
SD Pin -> Pico Pin
CS     -> GP3
SCK    -> GP6
MOSI   -> GP7
MISO   -> GP0
VCC    -> 3.3V
GND    -> GND
```

**Test con multimetro**:
- VCC = 3.3V ±0.1V
- GND = 0V
- Verifica continuità sui pin

**Prova**:
1. Altra SD card
2. Riformatta FAT32
3. Usa adattatore SD di qualità

### Problema: File non trovati

1. Elenca file con `l`
2. Verifica nomi esatti: `track1.mp3` (lowercase)
3. Verifica SD in PC - vedi i file?

### Problema: Decode FAILED

**Causa ID3 Tag**:
Alcuni MP3 hanno tag ID3v2 all'inizio. Il decoder dovrebbe saltarlo automaticamente.

**Test**:
```bash
# Rimuovi ID3 tag
ffmpeg -i input.mp3 -codec:a copy -id3v2_version 0 output.mp3
```

**Ricodifica MP3**:
```bash
# Crea MP3 pulito 192kbps 44.1kHz stereo
ffmpeg -i input.mp3 -ar 44100 -ac 2 -b:a 192k output.mp3
```

### Problema: Audio distorto/veloce/lento

**Sample Rate Mismatch**:
La libreria pico-audio è ottimizzata per 44.1kHz.

**Soluzione**:
Ricodifica a 44.1kHz:
```bash
ffmpeg -i input.mp3 -ar 44100 output.mp3
```

### Problema: Nessun audio dal DAC

**Verifica pin I2S**:
- BCK (bit clock) = GP20
- LRCK (word select) = GP21
- DIN (data) = GP22

**Verifica PCM5102**:
- VCC = 3.3V
- GND connesso
- Output connesso a speaker/cuffie

**Jumper PCM5102** (se presenti):
- H1L = Low (3.3V)
- H2L = Low
- H3L = Low
- H4L = Low

## 📊 Output Normale

Durante playback dovresti vedere:

```
▶ Playing: track1.mp3
  File size: 3458291 bytes
  ✓ Started
♪ Playing | Buf: 87% | Frames: 125 | Samples: 144000 | Pos: 131072/3458291 (3%)
♪ Playing | Buf: 89% | Frames: 251 | Samples: 288000 | Pos: 262144/3458291 (7%)
♪ Playing | Buf: 91% | Frames: 377 | Samples: 432000 | Pos: 393216/3458291 (11%)
...
✓ Playback finished
```

**Valori attesi**:
- Buf: 70-95% (buffer fill)
- Frames: aumenta costantemente
- Pos: progresso file

## 🔍 Differenze vs Versione SDIO

| Feature | SDIO Version | SPI Debug Version |
|---------|--------------|-------------------|
| Interface | SDIO 4-bit | SPI (standard) |
| Bandwidth | 10-12 MB/s | 1-2 MB/s |
| Pin count | 6 pins | 4 pins |
| Cores | Dual-core | Single-core |
| Players | 10 simultanei | 1 solo |
| Debug | Minimo | Massivo |
| Scopo | Produzione | Debugging |

## ✅ Se Tutto Funziona

Se questa versione SPI funziona correttamente:
1. ✅ Hardware OK (SD, DAC, wiring)
2. ✅ File MP3 OK
3. ✅ Decoder OK

**Prossimo step**: Passa alla versione SDIO per performance migliori e player multipli.

Se questa versione SPI **non** funziona:
- Usa i comandi debug (`i`, `l`, `t`, `d`)
- Verifica step-by-step quale fase fallisce
- Controlla wiring e hardware

## 📝 Note Tecniche

### Single Core
Questa versione usa solo Core0 per semplificare il debug. Tutto avviene sequenzialmente:
```
loop() {
  fillBufferFromMP3();  // SD read + decode
  sendToQueue();         // Send to audio
}
```

### Buffer Size
- MP3 buffer: 4KB (dati MP3 raw)
- Audio buffer: 8K samples (~186ms @ 44.1kHz)
- Sufficiente per SPI con 1 player

### Performance
Con SPI e single-core:
- CPU: ~15-20%
- RAM: ~80KB
- Players: 1 solo (OK per debug)

---

**Versione**: 1.0 DEBUG
**Scopo**: Diagnostica e verifica hardware/software
**Testato su**: Raspberry Pi Pico 2 (RP2350)
