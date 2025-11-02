# SD Card Diagnostic Tool - SPI

**Strumento di diagnostica completa per SD card** - niente audio, niente MP3, solo test della SD card.

## 🎯 Scopo

Questo tool verifica **esclusivamente** la connessione e il funzionamento della SD card, isolando completamente qualsiasi problema hardware o di configurazione.

## 📋 Hardware Necessario

### Raspberry Pi Pico 2 (RP2350)
Qualsiasi variante

### SD Card con adattatore SPI
```
SD Card Pin -> Pico Pin
CS          -> GP3
SCK         -> GP6
MOSI        -> GP7
MISO        -> GP0
VCC         -> 3.3V
GND         -> GND
```

**NESSUN ALTRO HARDWARE NECESSARIO** - né DAC, né cuffie, né altro.

## 🚀 Come Usare

1. **Upload** lo sketch su Raspberry Pi Pico 2
2. **Apri Serial Monitor** (115200 baud)
3. **Leggi l'output** di diagnostica automatica
4. **Usa comandi** interattivi per test dettagliati

## 📊 Output Normale

Se tutto funziona, vedrai:

```
╔═══════════════════════════════════════════╗
║  SD CARD DIAGNOSTIC TOOL - SPI            ║
║  v1.0 - Complete SD Card Testing          ║
╚═══════════════════════════════════════════╝

This tool tests ONLY the SD card connection.
No audio, no MP3 - just pure SD diagnostics.

╔═══ SPI CONFIGURATION ═══╗
  Setting up SPI... OK

  Pin Configuration:
    CS   -> GP3
    SCK  -> GP6
    MOSI -> GP7
    MISO -> GP0

╔═══ SD CARD INITIALIZATION ═══╗
  Initializing SD card... ✓ SUCCESS!

  Card Information:
    Size: 8192 MB
    Type: SDHC (2-32GB)

  Space:
    Total: 8192 MB
    Used:  245 MB (2%)
    Free:  7947 MB (97%)

╔═══ FILE LISTING ═══╗
  [FILE] track1.mp3                   3458291 bytes  ✓ MP3
  [FILE] track2.mp3                   2891234 bytes  ✓ MP3

  Total: 2 files, 0 directories, 6201 KB

╔═══════════════════════════════════════════╗
║  READY - Type 'h' for help               ║
╚═══════════════════════════════════════════╝
```

**✅ Se vedi questo**: La tua SD card funziona perfettamente!

## ❌ Se Vedi Errori

### Error: Cannot initialize SD card

```
╔═══ SD CARD INITIALIZATION ═══╗
  Initializing SD card... ❌ FAILED!

  ERROR: Cannot initialize SD card

  Troubleshooting checklist:
  □ SD card is inserted correctly
  □ SD card is formatted (FAT32 recommended)
  □ Check wiring: ...
```

**Causa**: La SD card non risponde.

**Soluzioni**:
1. ✅ **Verifica wiring** con multimetro:
   - CS → GP3
   - SCK → GP6
   - MOSI → GP7
   - MISO → GP0
   - VCC = 3.3V (misura con multimetro)
   - GND = 0V

2. ✅ **Prova altra SD card** (per escludere SD difettosa)

3. ✅ **Riformatta SD** in FAT32:
   - Windows: `format X: /FS:FAT32`
   - Mac: Disk Utility → Erase → MS-DOS (FAT)
   - Linux: `sudo mkfs.vfat -F 32 /dev/sdX`

4. ✅ **Verifica adattatore SD** (alcuni sono di bassa qualità)

5. ✅ **Prova alimentazione esterna** per SD card (alcuni Pico non forniscono abbastanza corrente)

## 🔧 Comandi Interattivi

Dopo l'inizializzazione, puoi digitare questi comandi nel Serial Monitor:

| Comando | Descrizione |
|---------|-------------|
| `i` | **Info** - Mostra info SD card (size, type, spazio) |
| `l` | **List** - Elenca tutti i file sulla SD |
| `r` | **Read test** - Legge primi 512 byte del primo file |
| `w` | **Write test** - Crea file `test.txt` |
| `d` | **Delete** - Cancella file `test.txt` |
| `f` | **Format info** - Info su filesystem |
| `s` | **Speed test** - Test velocità lettura/scrittura |
| `h` | **Help** - Mostra help comandi |

### Esempio: Read Test

Digita `r`:

```
╔═══ READ TEST ═══╗
  Testing file: track1.mp3
  File size: 3458291 bytes
  Reading first 512 bytes...
  Read 512 bytes in 1523 μs

  First 128 bytes (hex):
    0: 49 44 33 04 00 00 00 1F 76 5D 54 50 45 31 00 00
   16: 00 0F 00 00 03 4C 61 76 66 35 39 2E 32 37 2E 31
   32: 00 54 49 54 32 00 00 00 0E 00 00 03 54 65 73 74
  ...

  File signature check:
    ✓ ID3 tag (MP3 with metadata)

  ✓ Read test complete
```

**✅ Se riesci a leggere**: La SD funziona correttamente!

### Esempio: Speed Test

Digita `s`:

```
╔═══ SPEED TEST ═══╗
  Write Speed Test:
    Creating 100KB file... ✓ 287.4 KB/s
  Read Speed Test:
    Reading 100KB file... ✓ 512.8 KB/s

  Speed classification:
    ✓ Good (>500 KB/s)
```

**Interpretazione**:
- ✅ **>500 KB/s**: Ottimo per audio
- ⚠️ **200-500 KB/s**: Sufficiente ma limite
- ❌ **<200 KB/s**: Troppo lento - cambia SD card

### Esempio: Write Test

Digita `w`:

```
╔═══ WRITE TEST ═══╗
  Creating test.txt...
  Written 67 bytes

  Verification - file content:
  SD Card Write Test
  Timestamp: 12345 ms
  This is a test file.

  ✓ Write test complete
```

**✅ Se funziona**: SD card è scrivibile (non in read-only)

## 🔍 Casi d'Uso

### Caso 1: Verifica Wiring
1. Upload questo sketch
2. Leggi output iniziale
3. Se FAILED → problema wiring

### Caso 2: Verifica SD Card
1. Esegui speed test (`s`)
2. Se <200 KB/s → SD troppo lenta
3. Prova altra SD

### Caso 3: Verifica File MP3
1. Esegui list (`l`)
2. Verifica presenza file `track1.mp3`, etc.
3. Esegui read test (`r`)
4. Verifica signature ID3 o MP3

### Caso 4: Debug Formato
1. Esegui format info (`f`)
2. Verifica tipo card
3. Riformatta se necessario

## 📝 Checklist Debug Completa

**Prima di usare MP3 player**, verifica con questo tool:

- [ ] SD card si inizializza (`INITIALIZATION` → `SUCCESS`)
- [ ] File MP3 sono visibili (`l` → vedi file `.mp3`)
- [ ] Read test funziona (`r` → legge 512 bytes)
- [ ] Write test funziona (`w` → crea file)
- [ ] Speed test >200 KB/s (`s` → velocità OK)

**Se tutti i test passano**: Il problema NON è la SD card! Puoi passare al MP3 player.

**Se qualche test fallisce**: Risolvi il problema SD prima di procedere.

## ⚡ Performance Attese

### SD Card Class 4-10
- Read: 200-800 KB/s (SPI mode)
- Write: 100-500 KB/s (SPI mode)

### Per Audio MP3
- Bitrate 320kbps = 40 KB/s richiesti
- Con 200 KB/s puoi riprodurre **5 MP3 simultanei**
- Con 500 KB/s puoi riprodurre **12 MP3 simultanei**

## 🚫 Cosa NON Fa Questo Tool

- ❌ Non testa audio (nessun DAC richiesto)
- ❌ Non decodifica MP3 (solo lettura raw)
- ❌ Non usa I2S (solo SD card)
- ❌ Non usa dual-core (single core)

## ✅ Prossimi Step

**Se questo tool funziona perfettamente**:
1. Vai a `SDCardMP3PlayerSPI` per testare decoder MP3 + audio
2. Se anche quello funziona, passa a `SDCardMP3Player` (SDIO) per performance

**Se questo tool fallisce**:
1. Risolvi problema hardware (wiring, SD card, alimentazione)
2. Non procedere con MP3 player finché questo tool non funziona

---

**Versione**: 1.0
**Scopo**: Diagnostica SD card isolata
**Hardware**: Solo Pico 2 + SD card (niente DAC)
