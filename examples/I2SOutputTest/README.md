# I2S Output Test

Test diagnostico per verificare la configurazione I2S e PCM5102 DAC.

## 🎯 Scopo

Questo esempio genera un tono continuo a 440 Hz per verificare che:
- I2S sia configurato correttamente
- PCM5102 sia collegato ai pin giusti
- L'audio esca dal DAC

## 🔌 Pin I2S Predefiniti

```
Pimoroni Pico Plus → PCM5102
──────────────────────────────
GP20 → BCK (Bit Clock)
GP21 → LCK/LRCLK (Word Select)
GP22 → DIN (Data In)

3.3V → VIN (alimentazione)
GND  → GND
```

## ⚙️ Configurazione PCM5102

**IMPORTANTE**: I pin di configurazione del PCM5102 devono essere impostati così:

```
SCK pin → GND    (usa clock dal Pico, non master clock esterno)
FMT pin → GND    (formato I2S standard)
XMT pin → 3.3V   (unmute - FONDAMENTALE!)
         o FLOATING (non collegare, pull-up interno)
```

⚠️ **Se XMT è collegato a GND, il DAC è MUTO!**

## 🧪 Test

### 1. Carica l'esempio
```
File → Examples → pico-audio → I2SOutputTest
```

### 2. Apri Serial Monitor (115200 baud)

Dovresti vedere:
```
╔════════════════════════════════════╗
║   I2S Audio Output Test            ║
║   PCM5102 DAC Verification         ║
╚════════════════════════════════════╝

Starting I2S output...
✓ I2S initialized
✓ Test tone configured: 440 Hz sine wave

╔════════════════════════════════════╗
║  YOU SHOULD HEAR A TONE NOW!       ║
║  440 Hz (A4 musical note)          ║
╚════════════════════════════════════╝
```

### 3. Verifica Audio

✅ **SENTI IL TONO**: I2S funziona! Il problema è altrove
❌ **NON SENTI NULLA**: Verifica collegamenti (vedi sotto)

## 🎮 Comandi Interattivi

| Comando | Azione |
|---------|--------|
| `1-9` | Cambia frequenza (100-900 Hz) |
| `0` | Ritorna a 440 Hz (A4) |
| `+` | Aumenta volume |
| `-` | Diminuisce volume |
| `s` | Silenzio |

## 🐛 Troubleshooting

### Non sento nulla

**1. Verifica XMT pin**
```
Controlla con multimetro:
XMT pin dovrebbe essere a 3.3V o floating
Se è a 0V (GND) → DAC è muto!
```

**2. Verifica pin I2S**
```
Con oscilloscopio o logic analyzer:
GP20 (BCLK) → Dovrebbe avere clock a ~1.4 MHz
GP21 (LRCLK) → Dovrebbe avere clock a 44.1 kHz
GP22 (DIN) → Dovrebbe avere dati digitali
```

**3. Verifica alimentazione**
```
PCM5102 VIN: 3.3V
PCM5102 GND: 0V
Corrente: ~10-20mA
```

**4. Verifica output**
```
Prova diverse cuffie/speaker
Verifica volume cuffie
Controlla connettore jack (alcuni PCM5102 hanno jack)
```

**5. Verifica PCM5102**
```
Alcuni moduli PCM5102 hanno LED:
- LED acceso = alimentato correttamente
- LED che lampeggia = riceve dati I2S
```

### Sento rumore/distorsione

**1. Collegamenti instabili**
```
- Usa cavetti più corti
- Saldature fredde?
- Breadboard con cattivo contatto?
```

**2. Alimentazione rumorosa**
```
- Aggiungi condensatore 100uF su VIN
- Usa alimentazione pulita
```

**3. Ground loop**
```
- Verifica unico punto di massa
- Non mescolare GND digitale e analogico
```

## 📊 Output Atteso

```
♪ Freq: 440 Hz | Vol: 50% | CPU: 0.81% | Mem: 4
♪ Freq: 440 Hz | Vol: 50% | CPU: 0.81% | Mem: 4
♪ Freq: 440 Hz | Vol: 50% | CPU: 0.81% | Mem: 4
```

CPU dovrebbe essere < 1%
Memoria dovrebbe essere 4 blocchi

## 🔧 Pin Personalizzati

Se vuoi usare pin diversi, modifica il codice:

```cpp
// Invece di:
i2s1.begin();

// Usa:
i2s1.begin(GP_BCLK, GP_LRCLK, GP_DIN);
// Esempio: GP18, GP19, GP16
i2s1.begin(18, 19, 16);
```

## ✅ Se il Test Funziona

Se senti il tono con questo esempio, l'I2S è OK!

Il problema è allora:
- File WAV corrotti
- SD card non leggibile
- Formato WAV non supportato
- Buffer underrun nel player

Torna agli esempi WAV player con questa conferma.

## ❌ Se il Test NON Funziona

Controlla in ordine:
1. XMT pin (causa #1 di "no audio")
2. Pin I2S GP20/21/22
3. Alimentazione 3.3V
4. PCM5102 funzionante (prova altro modulo)
5. Output connesso correttamente

---

**Questo test è fondamentale per diagnosticare problemi I2S!**
