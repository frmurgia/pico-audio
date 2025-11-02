# Which SD Card WAV Player Should I Use?

There are several SD card WAV players in the examples folder. Here's a guide to help you choose the right one:

## 🎯 Quick Decision Guide

**For 10 large music files (>500KB each):**
→ Use **SDCardWavPlayerSDIO** (streaming player)

**For 10 short sound effects (<500KB each):**
→ Use **SDCardWavPlayerMemory** (memory-cached player)

**Just testing SD card basics:**
→ Use **SDCardWavPlayer** (simple SPI player)

---

## 📊 Detailed Comparison

### SDCardWavPlayerSDIO ⭐ RECOMMENDED FOR LARGE FILES
**Best for:** Music playback, large audio files, 10 simultaneous tracks

**Features:**
- ✅ SDIO 4-bit mode (10-12 MB/s bandwidth)
- ✅ Dual-core architecture (Core0=audio, Core1=SD operations)
- ✅ Streams from SD card (no file size limits)
- ✅ Can play 10 simultaneous large files
- ✅ Thread-safe circular buffers
- ✅ Works with files of ANY size

**Verified SDIO Pins:**
```
CLK:  GP7
CMD:  GP6
DAT0: GP8 (DAT1=GP9, DAT2=GP10, DAT3=GP11)
```

**Memory Usage:** ~40KB buffers (4KB × 10 players)

**Example Use Cases:**
- Background music + sound effects
- Multi-track music composition
- DJ mixing application
- Large audio files (>1MB)

---

### SDCardWavPlayerMemory
**Best for:** UI sounds, drum machines, instant playback

**Features:**
- ✅ SDIO 4-bit mode (fast loading)
- ✅ Loads files to RAM at startup
- ✅ Zero latency playback (no SD access during playback)
- ✅ Perfect sync for rhythm/drums
- ⚠️ File size limit: 500KB per file
- ⚠️ Total memory limit: 2MB

**Verified SDIO Pins:** Same as above (GP7, GP6, GP8-11)

**Memory Usage:** Actual file sizes (up to 2MB total)

**Example Use Cases:**
- Drum machine (10 drum samples)
- UI sound effects
- Short voice prompts
- Button click sounds
- Game sound effects

**Limitations:**
- Files >500KB are automatically skipped
- Total of all files must be <2MB
- Files must fit in available RAM

---

### SDCardWavPlayer (Basic SPI Version)
**Best for:** Testing, simple single-file playback

**Features:**
- ✅ Simple SPI interface (4 pins only)
- ✅ Easy to understand code
- ❌ Slow (1-2 MB/s)
- ❌ Can't handle 10 simultaneous files reliably

**SPI Pins:**
```
CS:   GP17
SCK:  GP18
MOSI: GP19
MISO: GP16
```

**Use when:**
- Just testing SD card functionality
- Playing 1-2 files at a time
- Learning how WAV playback works

---

### Other Players (Legacy/Testing)

- **SDCardWavPlayerOptimized**: Early SPI optimization attempt (obsolete)
- **SDCardWavPlayerDualCore**: Dual-core SPI version (superseded by SDIO)
- **SDCardWavPlayerSPIOptimized**: Ultra-optimized SPI (still not enough for 10 files)

These are kept for reference but **SDCardWavPlayerSDIO** is superior.

---

## 🎵 Your Use Case: 10 Simultaneous Music Files

Based on your debug output showing:
- `track1.wav` = 49870 KB (48 MB)
- `track3.wav` = 4112 KB (4 MB)

**Recommendation:** Use **SDCardWavPlayerSDIO**

**Why:**
1. Files are too large for memory-cached player (>500KB limit)
2. SDIO provides 10-12 MB/s (enough for 10 tracks @ 1.76 MB/s total)
3. Dual-core architecture prevents SD operations from blocking audio
4. Streaming means unlimited file sizes

---

## 🔧 Troubleshooting

### "No audio output" with SDCardWavPlayerSDIO

1. **Check SDIO pins:** Must use CLK=7, CMD=6, DAT0=8
2. **Check I2S DAC pins:** BCK=20, LRCK=21, DIN=22
3. **Check serial output:** Look for "Track X playing"
4. **Check file format:** Must be 16-bit PCM, 44.1kHz
5. **Test with single file first:** Press '1' to play track1.wav

### "Files skipped" with SDCardWavPlayerMemory

This is normal! Files >500KB are automatically skipped. Use SDCardWavPlayerSDIO instead.

### "SD card initialization failed"

1. Check SDIO wiring
2. Try different SD card (some cards don't support SDIO)
3. Check power supply (SD cards need stable 3.3V)
4. Run SDCardSDIODiagnostic to test SDIO functionality

---

## 📝 Summary Table

| Player | Interface | Speed | Max Files | File Size | Best For |
|--------|-----------|-------|-----------|-----------|----------|
| **SDCardWavPlayerSDIO** | SDIO 4-bit | 10-12 MB/s | 10 | Unlimited | **Large music files** ⭐ |
| **SDCardWavPlayerMemory** | SDIO 4-bit | 10-12 MB/s | 10 | 500KB each | **Short sound effects** |
| SDCardWavPlayer | SPI | 1-2 MB/s | 1-2 | Unlimited | Testing only |

---

**Created:** 2025-11-02
**Last Updated:** 2025-11-02
**Verified SDIO Pins:** CLK=GP7, CMD=GP6, DAT0=GP8 (DAT1-3: GP9-11)
