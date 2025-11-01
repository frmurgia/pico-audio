# SDIO WAV Player - PlatformIO Project

10-channel simultaneous WAV file player for RP2350 using SDIO 4-bit mode.

## Hardware Requirements

### Board
- **Pimoroni Pico Plus 2** (RP2350B with 8MB PSRAM)
- Or any RP2350 board with PSRAM

### DAC
- **PCM5102** I2S DAC module

### SD Card
- MicroSD card with SDIO support (most modern cards)
- Formatted as FAT32
- Contains files: `track1.wav`, `track2.wav`, ..., `track10.wav`

## Pin Connections

### I2S (PCM5102 DAC)
```
PCM5102 → RP2350
─────────────────
BCK  → GP20
LRCK → GP21
DIN  → GP22
VCC  → 3.3V
GND  → GND
FMT  → GND
XMT  → 3.3V
```

### SDIO (SD Card - 6 pins)
```
SD Card → RP2350
────────────────
CLK  → GP10
CMD  → GP11
DAT0 → GP12  ⚠️ Must be consecutive!
DAT1 → GP13
DAT2 → GP14
DAT3 → GP15
VCC  → 3.3V
GND  → GND
```

**⚠️ CRITICAL:** DAT0-3 pins must be on consecutive GPIOs!

## Building and Uploading

### Prerequisites
```bash
pip install platformio
```

### Build
```bash
cd platformio-sdio-wavplayer
pio run
```

### Upload
```bash
pio run --target upload
```

### Monitor
```bash
pio device monitor
# or
pio run --target monitor
```

## Usage

### Serial Commands
- `'1'` to `'9'`, `'0'` - Play tracks 1-10
- `'s'` - Stop all tracks
- `'l'` - List WAV files
- `'d'` - Show debug info

### Example Session
```
╔════════════════════════════════════════╗
║  SD WAV Player - SDIO 4-BIT MODE     ║
║  VERSION 2.3 (2025-11-01)             ║
║  RP2350B - 10-12 MB/s SDIO Bandwidth ║
╚════════════════════════════════════════╝

Core1: Initializing SD (SDIO mode)... OK
Core1: SD card size: 8192 MB
Core1: SDIO 4-bit mode active (10-12 MB/s)
SD card: OK (SDIO mode)

Ready!
Commands: '1'-'0' = play track, 's' = stop, 'l' = list, 'd' = debug

> 1
▶ Loading track 1
♪ Players: 1 | CPU: 2% | Mem: 80 | Buf: 75%

> 2
▶ Loading track 2
♪ Players: 2 | CPU: 3% | Mem: 82 | Buf: 72%
```

## WAV File Requirements

- **Format:** PCM 16-bit
- **Sample Rate:** 44.1 kHz (recommended)
- **Channels:** Mono or Stereo (stereo is mixed to mono)
- **Naming:** `track1.wav`, `track2.wav`, ..., `track10.wav`

## Performance

- **SDIO Bandwidth:** 10-12 MB/s
- **Required for 10 files @ 44.1kHz stereo:** 1.76 MB/s
- **Headroom:** 6x safety margin
- **CPU Usage:** ~5% with 10 simultaneous tracks
- **Memory:** ~87/120 blocks

## Architecture

### Dual-Core Design
- **Core0:** Audio processing only (mixers, I2S output)
- **Core1:** SD card operations only (file reading, buffering)
- **Communication:** Thread-safe circular buffers with mutexes

### Buffer Configuration
- 8KB circular buffer per player (186ms audio)
- 120 audio memory blocks total
- No buffer underruns with SDIO speed

## Troubleshooting

### SD Card Not Detected
1. Check all 6 SDIO pins are connected
2. Verify DAT0-3 are on consecutive GPIOs (12-15)
3. Try a different SD card (some old cards are SPI-only)
4. Check power (3.3V and GND)

### Audio Distortion
1. Check I2S pin connections (GP20, GP21, GP22)
2. Verify PCM5102 VCC is 3.3V (not 5V!)
3. Check for ground loops

### Buffer Underruns
If "Buf: 0%" appears:
1. SD card may not support SDIO
2. Use diagnostic tool: `../SDCardDiagnostic/`
3. Fall back to SPI version if needed

## Project Structure

```
platformio-sdio-wavplayer/
├── platformio.ini          # PlatformIO configuration
├── src/
│   └── main.cpp           # Main WAV player code
├── lib/
│   └── pico-audio/        # Audio library (local)
└── README.md              # This file
```

## License

Based on Teensy Audio Library by Paul Stoffregen (PJRC)
Ported to RP2040/RP2350 with I2S support

## Version

**2.3** - Fixed race condition in SDIO initialization (2025-11-01)

## Links

- [pico-audio GitHub](https://github.com/frmurgia/pico-audio)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
