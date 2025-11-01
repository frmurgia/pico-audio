# AudioPlayQueue Test

Test diagnostico per verificare il funzionamento di **AudioPlayQueue**.

## 🎯 Scopo

Questo test genera audio sintetico usando **AudioPlayQueue** (lo stesso componente usato dal WAV player) per isolare il problema.

**Path testato:**
```
Dati Sintetici → AudioPlayQueue → I2S → PCM5102
```

Questo è identico al WAV player, tranne che i dati vengono generati in RAM invece di essere letti da SD.

## 🧪 Test

### Carica l'esempio
```
File → Examples → pico-audio → PlayQueueTest
Upload
Serial Monitor @ 115200
```

### Risultati Possibili

**✅ SENTI IL TONO (440 Hz)**
```
→ AudioPlayQueue funziona!
→ Il path queue → I2S è OK
→ Il problema è nella lettura WAV da SD:
  - File WAV corrotto/invalido
  - Parsing WAV header sbagliato
  - Dati letti da SD sono zero
  - Buffer non viene riempito da Core1
```

**❌ NON SENTI NULLA**
```
→ Problema con AudioPlayQueue
→ Possibili cause:
  - AudioMemory troppo basso
  - Timing getBuffer/playBuffer
  - Bug in AudioPlayQueue implementation
```

## 📊 Output Atteso

```
╔════════════════════════════════════╗
║   AudioPlayQueue Test              ║
║   Synthetic Data Generation        ║
╚════════════════════════════════════╝

✓ I2S started
✓ AudioPlayQueue initialized

╔════════════════════════════════════╗
║  YOU SHOULD HEAR A TONE NOW!       ║
║  Generated via AudioPlayQueue      ║
╚════════════════════════════════════╝

♪ Buffers: 1000 | CPU: 1.5% | Mem: 6
♪ Buffers: 2000 | CPU: 1.5% | Mem: 6
```

Se vedi questo output e senti il tono → AudioPlayQueue funziona!

---

## 🔍 Prossimi Step Basati sui Risultati

### Se AudioPlayQueue Funziona

Il problema è nel **WAV player**. Verifica:

1. **File WAV valido?**
```
- Formato: PCM (non compressed)
- Bit depth: 16-bit
- Sample rate: 44100 Hz
- Testa con file generato da Audacity
```

2. **Debug Core1** nel dual-core player:
```
Aggiungi print in core1_fillBuffer():
Serial.print("Core1: Read ");
Serial.print(samplesToRead);
Serial.println(" samples");
```

3. **Verifica dati letti**:
```
Aggiungi in core1_fillBuffer():
Serial.print("Sample[0]: ");
Serial.println(player->buffer[0]);
```

Se stampa sempre 0 → file WAV è silenzioso o parsing sbagliato

### Se AudioPlayQueue NON Funziona

Problema più profondo con la libreria. Invia:
- Output seriale completo
- Versione pico-audio
- Versione Arduino RP2040 core
