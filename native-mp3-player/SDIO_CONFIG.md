# Configurazione SDIO Nativa

## Pin Configuration

I pin SDIO configurati in questo progetto sono:

```cpp
// In main.cpp
#define SD_CLK_PIN  6   // Clock
#define SD_CMD_PIN  11  // Command
#define SD_DAT0_PIN 0   // Data 0 (D1=1, D2=2, D3=3 consecutivi)
```

Questo corrisponde alla configurazione SDIO nativa:

```c
static sd_sdio_if_t sdio_if = {
    .CLK_gpio      = 6,
    .CMD_gpio      = 11,
    .D0_gpio       = 0,     // D1=1, D2=2, D3=3 automatici
    .SDIO_PIO      = pio1,
    .DMA_IRQ_num   = DMA_IRQ_1,
    .baud_rate     = (125 * 1000 * 1000) / 6  // ~20.8 MHz
};
```

## Wiring

```
SD Card Pin → Raspberry Pi Pico 2 Pin
───────────────────────────────────────
CLK         → GP6
CMD         → GP11
DAT0        → GP0  ⚠️ IMPORTANTE: Consecutivi!
DAT1        → GP1
DAT2        → GP2
DAT3        → GP3
VCC         → 3.3V
GND         → GND
```

## ⚠️ IMPORTANTE: Pin Consecutivi

I pin DAT0-DAT3 **DEVONO** essere consecutivi per il funzionamento SDIO 4-bit:
- DAT0 = GP0
- DAT1 = GP1 (DAT0 + 1)
- DAT2 = GP2 (DAT0 + 2)
- DAT3 = GP3 (DAT0 + 3)

**Non puoi usare pin sparsi!**

## PIO e DMA

La configurazione usa:
- **PIO1**: Gestisce la comunicazione SDIO
- **DMA_IRQ_1**: Interrupt DMA dedicato
- **Baud Rate**: ~20.8 MHz (125MHz / 6)

## Libreria Utilizzata

Questo progetto usa la libreria Arduino SD.h con backend SDIO, che internamente configura l'hardware SDIO del RP2350.

Se vuoi usare **no_OS_FatFS_SDIO** direttamente (più controllo, no Arduino):
1. Clona: https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
2. Configura `hw_config.c` con i pin sopra
3. Usa FatFS API invece di Arduino SD.h

## Vantaggi SDIO vs SPI

| Feature | SPI | SDIO 4-bit |
|---------|-----|------------|
| Bandwidth | 1-2 MB/s | 10-12 MB/s |
| Pin richiesti | 4 | 6 |
| CPU overhead | Medio | Basso (DMA) |
| Latenza | Alta | Bassa |

Per file MP3 grandi (20MB+), SDIO è **essenziale**.

## Test Pin Configuration

Per verificare che i pin siano corretti, all'avvio vedrai:

```
Launching Core1... OK
```

Se vedi errore, verifica:
1. Pin DAT0-3 siano consecutivi
2. Nessun altro device usi GP0-3, GP6, GP11
3. SD card sia inserita correttamente
4. Alimentazione 3.3V stabile

## Modifica Pin

Se vuoi cambiare i pin SDIO, modifica in `src/main.cpp`:

```cpp
#define SD_CLK_PIN  <nuovo_pin_clock>
#define SD_CMD_PIN  <nuovo_pin_command>
#define SD_DAT0_PIN <nuovo_pin_d0>  // D1,D2,D3 auto
```

**Ricorda**: DAT0 deve poter avere 3 pin consecutivi dopo di sé!

Esempi validi:
- DAT0=0 → D1=1, D2=2, D3=3 ✅
- DAT0=8 → D1=9, D2=10, D3=11 ✅
- DAT0=20 → D1=21, D2=22, D3=23 ✅

Esempi NON validi:
- DAT0=27 → D3=30 (oltre GP29) ❌
- DAT0=5 con GP6 usato per altro ❌

## Troubleshooting Pin

### SD Initialization Failed

```bash
# Verifica continuità pin con multimetro
GP6  → SD CLK  (resistenza < 1Ω)
GP11 → SD CMD  (resistenza < 1Ω)
GP0  → SD DAT0 (resistenza < 1Ω)
...
```

### Pin Conflict

Se altri periferiche usano GP0-3:
- Libera quei pin, oppure
- Cambia DAT0_PIN a range libero (es: GP8-11)

---

**Ultima modifica**: 2025-11-02
**Pin testati**: CLK=6, CMD=11, D0=0 (funzionanti)
