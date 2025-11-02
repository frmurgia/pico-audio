# Native C MP3 Player - Pico SDK

**VERO progetto C nativo** usando pico-sdk direttamente - **NO Arduino**!

## 🎯 Caratteristiche

- ✅ **Pure C**: Niente C++, niente Arduino
- ✅ **pico-sdk**: API nativa Raspberry Pi
- ✅ **SDIO nativo**: Configurazione manuale con no_OS_FatFS
- ✅ **I2S PIO**: Implementazione diretta con PIO
- ✅ **Dual-core**: Gestione manuale multicore
- ✅ **File grandi**: Streaming ottimizzato

## 📋 Requisiti

### Software
```bash
# 1. pico-sdk
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init
export PICO_SDK_PATH=$(pwd)

# 2. Toolchain
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi \
                 build-essential libstdc++-arm-none-eabi-newlib

# 3. no_OS_FatFS_SDIO (per SD card)
cd /path/to/mp3-player-native-c/lib
git clone https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.git fatfs
```

### Hardware
- Raspberry Pi Pico 2 (RP2350)
- SD Card con SDIO
- DAC I2S (PCM5102 o compatibile)

## 🔌 Wiring

### SDIO (SD Card)
```
SD Pin  → Pico Pin
CLK     → GP6
CMD     → GP11
DAT0    → GP0  ⚠️ Consecutivi!
DAT1    → GP1
DAT2    → GP2
DAT3    → GP3
VCC     → 3.3V
GND     → GND
```

### I2S (PCM5102)
```
DAC Pin → Pico Pin
BCK     → GP20
LRCK    → GP21
DIN     → GP22
VCC     → 3.3V
GND     → GND
```

## 🚀 Build

### 1. Setup Environment
```bash
export PICO_SDK_PATH=/path/to/pico-sdk
```

### 2. Crea Build Directory
```bash
cd mp3-player-native-c
mkdir build
cd build
```

### 3. CMake Configure
```bash
cmake ..
```

### 4. Build
```bash
make -j4
```

### 5. Flash
```bash
# Metti Pico in BOOTSEL mode (tieni premuto BOOTSEL mentre colleghi USB)
cp mp3_player.uf2 /media/$USER/RPI-RP2/
```

## 📁 Struttura Progetto

```
mp3-player-native-c/
├── CMakeLists.txt          # Build configuration
├── src/
│   ├── main.c              # Main program + dual-core
│   ├── audio_i2s.c         # I2S implementation (PIO)
│   ├── audio_i2s.h
│   ├── mp3_decoder.c       # minimp3 wrapper
│   ├── mp3_decoder.h
│   └── minimp3.h           # MP3 decoder library
├── lib/
│   └── fatfs/              # no_OS_FatFS_SDIO (da clonare)
└── README.md
```

## ⚙️ Configurazione

### Pin SDIO

In `src/main.c`:
```c
#define SDIO_CLK_PIN  6
#define SDIO_CMD_PIN  11
#define SDIO_D0_PIN   0   // D1=1, D2=2, D3=3
```

### Pin I2S

In `src/main.c`:
```c
#define I2S_BCK_PIN   20  // Bit clock
#define I2S_LRCK_PIN  21  // Left/Right clock
#define I2S_DIN_PIN   22  // Data
```

### Buffer Size

In `src/main.c`:
```c
#define AUDIO_BUFFER_SIZE (32768)  // 32KB = ~743ms @ 44.1kHz
#define MP3_BUFFER_SIZE (8192)     // 8KB MP3 buffer
```

## 🔧 Implementazione Completa

### Questo è uno SCHELETRO

Il progetto corrente è un **template base**. Devi completare:

#### 1. SDIO con no_OS_FatFS

In `src/main.c`, funzione `init_sdio()`:

```c
#include "ff.h"  // From no_OS_FatFS
#include "sd_card.h"

void init_sdio(void) {
    static sd_sdio_if_t sdio_if = {
        .CLK_gpio      = 6,
        .CMD_gpio      = 11,
        .D0_gpio       = 0,
        .SDIO_PIO      = pio1,
        .DMA_IRQ_num   = DMA_IRQ_1,
        .baud_rate     = (125 * 1000 * 1000) / 6  // ~20.8 MHz
    };

    // Inizializza SD card
    sd_init_driver();
    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        printf("SD mount failed: %d\n", fr);
    }
}
```

#### 2. I2S con PIO

Crea `src/i2s.pio`:

```pio
.program i2s
.side_set 2

; I2S output program
; Bit 0: BCK (bit clock)
; Bit 1: LRCK (word select)

public entry_point:
    pull            side 0b00  ; Pull sample from FIFO
    set x, 14       side 0b00  ; 15 bits to send (left channel)
left_loop:
    out pins, 1     side 0b01  ; Output bit, BCK high
    jmp x-- left_loop side 0b00  ; BCK low, decrement

    set x, 14       side 0b10  ; Right channel, LRCK high
right_loop:
    out pins, 1     side 0b11  ; Output bit, BCK high
    jmp x-- right_loop side 0b10  ; BCK low

.wrap
```

Poi compila con:
```bash
pioasm -o c-sdk src/i2s.pio src/i2s.pio.h
```

E implementa in `audio_i2s.c`:
```c
#include "i2s.pio.h"

void audio_i2s_init(uint bck_pin, uint lrck_pin, uint din_pin, uint32_t sample_rate) {
    // Load PIO program
    uint offset = pio_add_program(i2s_pio, &i2s_program);

    // Configure state machine
    pio_sm_config c = i2s_program_get_default_config(offset);
    sm_config_set_out_pins(&c, din_pin, 1);
    sm_config_set_sideset_pins(&c, bck_pin);

    // Set pin directions
    pio_gpio_init(i2s_pio, din_pin);
    pio_gpio_init(i2s_pio, bck_pin);
    pio_gpio_init(i2s_pio, lrck_pin);

    // Set clock divider for sample rate
    float div = (float)clock_get_hz(clk_sys) / (sample_rate * 32 * 2);
    sm_config_set_clkdiv(&c, div);

    // Initialize and start
    pio_sm_init(i2s_pio, i2s_sm, offset, &c);
    pio_sm_set_enabled(i2s_pio, i2s_sm, true);
}
```

#### 3. Lettura File MP3

In `src/main.c`, funzione `load_mp3_file()`:

```c
bool load_mp3_file(const char* filename) {
    FIL fil;
    FRESULT fr = f_open(&fil, filename, FA_READ);
    if (fr != FR_OK) {
        printf("Failed to open: %s\n", filename);
        return false;
    }

    player.file = fil;  // Store handle
    player.file_size = f_size(&fil);
    player.playing = true;

    return true;
}
```

E in `process_mp3_frame()`:
```c
void process_mp3_frame(void) {
    // Read from SD
    UINT bytes_read;
    uint8_t read_buf[512];
    f_read(&player.file, read_buf, sizeof(read_buf), &bytes_read);

    // Append to MP3 buffer
    memcpy(player.mp3_buffer + player.mp3_fill, read_buf, bytes_read);
    player.mp3_fill += bytes_read;

    // Decode frame
    int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    mp3_frame_info_t info;

    int samples = mp3_decoder_decode_frame(&decoder,
                                            player.mp3_buffer,
                                            player.mp3_fill,
                                            pcm,
                                            &info);

    if (samples > 0) {
        // Convert stereo to mono if needed
        if (info.channels == 2) {
            int16_t mono[samples/2];
            mp3_decoder_convert_to_mono(pcm, samples, mono);
            // Write mono to circular buffer...
        }

        // Remove decoded bytes
        memmove(player.mp3_buffer,
                player.mp3_buffer + info.frame_bytes,
                player.mp3_fill - info.frame_bytes);
        player.mp3_fill -= info.frame_bytes;
    }
}
```

## 📚 Risorse

### Documentazione
- [pico-sdk Documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
- [RP2350 Datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf)
- [no_OS_FatFS_SDIO](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)
- [minimp3](https://github.com/lieff/minimp3)

### Esempi PIO I2S
- [pico-extras/audio](https://github.com/raspberrypi/pico-extras/tree/master/src/rp2_common/pico_audio_i2s)
- [I2S PIO Examples](https://github.com/raspberrypi/pico-examples/tree/master/pio)

## 🔍 Debug

### Serial Monitor
```bash
# minicom
sudo minicom -b 115200 -D /dev/ttyACM0

# screen
screen /dev/ttyACM0 115200
```

### Output Atteso
```
╔════════════════════════════════════════╗
║  NATIVE C MP3 PLAYER                   ║
║  Raspberry Pi Pico 2 - Pure C          ║
╚════════════════════════════════════════╝

Configuration:
  SDIO: CLK=6 CMD=11 D0=0
  I2S:  BCK=20 LRCK=21 DIN=22
  Buffer: 32768 samples (743.0 ms @ 44100 Hz)

Initializing player... OK
Initializing SDIO... OK
Initializing I2S... OK
Launching Core1... OK

Ready! Type filename to play:
Example: track1.mp3

> track1.mp3
Loading: track1.mp3
Playing!
```

## ⚠️ Note

### Questo è un Template

Il codice fornito è una **struttura base** per mostrarti come organizzare un progetto C nativo.

**Implementazioni mancanti**:
- [ ] SDIO init con no_OS_FatFS
- [ ] I2S PIO program
- [ ] DMA per I2S
- [ ] File I/O con FatFS
- [ ] MP3 frame reading loop

### Perché Template?

Un'implementazione completa richiederebbe:
- ~2000 righe di codice C
- Configurazione PIO complessa
- Gestione DMA multi-channel
- State machine per FatFS
- Debugging hardware

Questo template ti dà la **struttura corretta** - puoi completarla seguendo gli esempi linkati sopra.

## 💡 Alternative

Se vuoi una soluzione **funzionante subito**, considera:
1. Usare `native-mp3-player` (Arduino/C++ hybrid) - funziona out of the box
2. Modificare esempi pico-extras/audio_i2s
3. Partire da no_OS_FatFS esempi e aggiungere MP3

Questo progetto C nativo è per chi vuole **massimo controllo** e **zero overhead Arduino**.

---

**Versione**: 1.0 Template
**Stato**: Scheletro - Richiede implementazione completa
**Difficoltà**: ⭐⭐⭐⭐⭐ (Avanzato - richiede conoscenza pico-sdk)
