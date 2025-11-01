# Esempi pico-audio

Questa cartella contiene esempi completi per tutti i moduli audio disponibili nella libreria pico-audio, un porting della Teensy Audio Library per Raspberry Pi Pico 2 (RP2350) con DAC PCM5102.

## Requisiti Hardware

- **Raspberry Pi Pico 2** o **Pico 2 W** (RP2350 richiesto - non funziona su RP2040)
- **DAC I2S PCM5102** collegato ai pin I2S
- **Cavi audio** per connettere il DAC a casse/cuffie

## Configurazione Arduino IDE

- Arduino IDE 2.3.6+
- Pico Arduino Core 4.5.3+
- Frequenza di campionamento: 44.1 kHz
- Dimensione blocco: 128 campioni

## Indice Esempi

### 🎹 Sintetizzatori (Synthesis)

| Esempio | Modulo | Descrizione |
|---------|--------|-------------|
| [SineWave](SineWave/) | `AudioSynthWaveformSine` | Oscillatore sinusoidale puro (440 Hz) |
| [SineWaveHires](SineWaveHires/) | `AudioSynthWaveformSineHires` | Oscillatore sinusoidale ad alta risoluzione con sweep |
| [SineWaveModulated](SineWaveModulated/) | `AudioSynthWaveformSineModulated` | Sintesi FM con modulazione di frequenza |
| [BasicWaveforms](BasicWaveforms/) | `AudioSynthWaveform` | Forme d'onda base: sine, square, triangle, sawtooth, pulse |
| [PWMSynthesis](PWMSynthesis/) | `AudioSynthWaveformPWM` | Sintesi PWM con duty cycle variabile |
| [WhiteNoise](WhiteNoise/) | `AudioSynthNoiseWhite` | Generatore rumore bianco |
| [PinkNoise](PinkNoise/) | `AudioSynthNoisePink` | Generatore rumore rosa (1/f) |
| [ToneSweep](ToneSweep/) | `AudioSynthToneSweep` | Sweep lineare di frequenza |
| [DCOffset](DCOffset/) | `AudioSynthWaveformDc` | Generatore DC per controllo e offset |
| [KarplusStrong](KarplusStrong/) | `AudioSynthKarplusStrong` | Sintesi fisica corde pizzicate |
| [SimpleDrum](SimpleDrum/) | `AudioSynthSimpleDrum` | Sintetizzatore percussioni semplice (4 drum) |

### 🎚️ Effetti Audio (Effects)

| Esempio | Modulo | Descrizione |
|---------|--------|-------------|
| [SimpleDelay](SimpleDelay/) | `AudioEffectDelay` | Delay/Echo con tempo variabile |
| [Reverb](Reverb/) | `AudioEffectReverb` | Riverbero base |
| [Freeverb](Freeverb/) | `AudioEffectFreeverb` | Riverbero Freeverb ad alta qualità |
| [Chorus](Chorus/) | `AudioEffectChorus` | Effetto chorus per suoni densi |
| [Flange](Flange/) | `AudioEffectFlange` | Flanger "jet plane" effect |
| [Bitcrusher](Bitcrusher/) | `AudioEffectBitcrusher` | Riduzione bit depth per suoni lo-fi |
| [Waveshaper](Waveshaper/) | `AudioEffectWaveshaper` | Distorsione waveshaping non lineare |
| [WaveFolder](WaveFolder/) | `AudioEffectWaveFolder` | Wave folding per armoniche complesse |
| [Rectifier](Rectifier/) | `AudioEffectRectifier` | Raddrizzamento (octave-up effect) |
| [Envelope](Envelope/) | `AudioEffectEnvelope` | Inviluppo ADSR |
| [FadeInOut](FadeInOut/) | `AudioEffectFade` | Fade in/out per transizioni morbide |
| [Granular](Granular/) | `AudioEffectGranular` | Sintesi granulare con freeze |
| [MidSideProcessing](MidSideProcessing/) | `AudioEffectMidSide` | Elaborazione mid-side stereo |
| [Multiply](Multiply/) | `AudioEffectMultiply` | Ring modulation (moltiplicazione) |
| [DigitalCombine](DigitalCombine/) | `AudioEffectDigitalCombine` | Operazioni bitwise (AND, OR, XOR) |

### 🎛️ Filtri (Filters)

| Esempio | Modulo | Descrizione |
|---------|--------|-------------|
| [BiquadFilter](BiquadFilter/) | `AudioFilterBiquad` | Filtro biquad (lowpass, highpass, bandpass, notch) |
| [FIRFilter](FIRFilter/) | `AudioFilterFIR` | Filtro FIR con coefficienti personalizzati |
| [StateVariableFilter](StateVariableFilter/) | `AudioFilterStateVariable` | Filtro state variable con risonanza |
| [LadderFilter](LadderFilter/) | `AudioFilterLadder` | Filtro ladder stile Moog |

### 📊 Analizzatori (Analysis)

| Esempio | Modulo | Descrizione |
|---------|--------|-------------|
| [FFTAnalyzer](FFTAnalyzer/) | `AudioAnalyzeFFT1024` | Analisi spettrale FFT 1024-point |
| [PeakDetector](PeakDetector/) | `AudioAnalyzePeak` | Rilevatore picchi per VU meter |
| [RMSAnalyzer](RMSAnalyzer/) | `AudioAnalyzeRMS` | Analizzatore RMS (potenza media) |
| [ToneDetector](ToneDetector/) | `AudioAnalyzeToneDetect` | Rilevatore di frequenza specifica |
| [NoteFrequency](NoteFrequency/) | `AudioAnalyzeNoteFrequency` | Rilevatore note musicali (guitar tuner) |
| [AudioPrint](AudioPrint/) | `AudioAnalyzePrint` | Stampa dati audio raw per debug |

### 🔊 Riproduzione (Playback)

| Esempio | Modulo | Descrizione |
|---------|--------|-------------|
| [SamplePlayer](SamplePlayer/) | `AudioPlayMemory` | Riproduzione campioni da memoria (6 sample) |
| [PlayQueue](PlayQueue/) | `AudioPlayQueue` | Generazione audio dinamica via code |

### 💾 SD Card (Lettura/Scrittura)

| Esempio | Moduli | Descrizione |
|---------|--------|-------------|
| [SDCardReadTest](SDCardReadTest/) | SD Card SPI | Test lettura SD: lista file, info card, test connettività |
| [SDCardWriteTest](SDCardWriteTest/) | SD Card SPI | Test scrittura SD: crea file, append, binary write |
| [SDCardWavPlayer](SDCardWavPlayer/) | `AudioPlayQueue`, SD | Riproduzione 10 file WAV simultanei da SD card |
| [SDCardWavPlayerOptimized](SDCardWavPlayerOptimized/) | `AudioPlayQueue`, SD | **OTTIMIZZATO**: Pre-buffering 16KB/player, smooth playback |

**Configurazione pin SD Card (SPI):**
- SCK (Clock) → GP6
- MOSI (Data In) → GP7
- MISO (Data Out) → GP4
- CS (Chip Select) → GP5

### 🎚️ Mixer & Amplificazione

| Esempio | Modulo | Descrizione |
|---------|--------|-------------|
| [Mixer4Channel](Mixer4Channel/) | `AudioMixer4` | Mixer a 4 canali con gain indipendente |
| [Amplifier](Amplifier/) | `AudioAmplifier` | Amplificatore/Attenuatore con ramping |

### 🎼 Esempi Complessi

| Esempio | Moduli Utilizzati | Descrizione |
|---------|-------------------|-------------|
| [PolySynth](PolySynth/) | `AudioSynthWaveform`, `AudioMixer4`, MIDI | Sintetizzatore polifonico MIDI (20 voci) |
| [DualCore](DualCore/) | Vari | Utilizzo dual-core del RP2350 |
| [MemoryAndCpuUsage](MemoryAndCpuUsage/) | Vari | Monitoraggio uso risorse |

## Struttura Tipica di un Esempio

```cpp
#include <pico-audio.h>

// Dichiara componenti audio
AudioSynthWaveform  osc1;
AudioOutputI2S      i2s1;
AudioConnection     patchCord1(osc1, 0, i2s1, 0);

void setup() {
  Serial.begin(115200);

  // Alloca memoria audio
  AudioMemory(10);

  // Configura moduli
  osc1.begin(0.5, 440, WAVEFORM_SINE);

  // Avvia output I2S
  i2s1.begin();
}

void loop() {
  // La tua logica
}
```

## Note Importanti

### Memoria Audio
Ogni esempio chiama `AudioMemory(N)` con un valore specifico. Questo alloca N blocchi di 128 sample ciascuno. Moduli che utilizzano delay o buffer grandi (FFT, Reverb, Delay) richiedono più memoria.

### Problemi Noti
- **FFT256**: Ha problemi su RP2350, usare FFT1024 invece
- **Performance**: RP2350 è ~5x più lento di Teensy 4.1
- **Input Audio**: Non ancora supportato (solo output I2S)

### Frequenza Campionamento
Tutti gli esempi funzionano a **44.1 kHz** con blocchi di **128 campioni**.

### Pin I2S Predefiniti
Controlla la documentazione di pico-audio per i pin I2S utilizzati dal tuo DAC PCM5102.

## Debug e Ottimizzazione

Molti esempi includono diagnostica CPU e memoria:

```cpp
Serial.print("CPU: ");
Serial.print(AudioProcessorUsageMax());
Serial.print("% Memory: ");
Serial.println(AudioMemoryUsageMax());
AudioProcessorUsageMaxReset();
```

## Risorse Aggiuntive

- **Repository**: https://github.com/ghostintranslation/pico-audio
- **Teensy Audio Library**: https://www.pjrc.com/teensy/td_libs_Audio.html
- **Documentazione RP2350**: https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf

## Contribuire

Hai creato un esempio interessante? Sentiti libero di aprire una Pull Request!

## Licenza

Vedi il file LICENSE del repository principale.

---

**Ultimo aggiornamento**: 2025-11-01
**Esempi totali**: 44
**Moduli coperti**: Tutti i moduli audio disponibili + SD Card I/O (con versione ottimizzata)
