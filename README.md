# Multi-Input Signal Recorder

This Arduino project that gets multiple analog inputs, mixes them into a single waveform, saves it as a WAV file on an SD card, visualizes the waveform in ASCII in the Serial Monitor, and plays back the recorded audio using a speaker.

---

## Features

- **Multiple analog inputs**: Reads from 4 analog pins (`A1`–`A4`).
- **Mixing**: Averages the signals to create a single 8-bit PCM waveform.
- **WAV recording**: Saves the audio to `signal.wav` on an SD card.
- **Visualization**:
  - ASCII waveform printed in Serial Monitor.
  - RMS (Root Mean Square) and peak amplitude.
  - Frequency estimation using zero-crossings.
- **Playback**: Uses the **TMRpcm** library to play the recorded WAV file through a speaker.

---

## Hardware Required

- Arduino Uno (or compatible)
- SD card module (CS → pin 10)
- Micro SD card
- Speaker (TMRpcm compatible, connected to pin 9)
- Analog signal sources (e.g., 555 timer ICs, potentiometers, sensors) on `A1`–`A4`
- Optional: Pull-down resistors if needed for stable analog readings

---

## Software Libraries

- [SD.h](https://www.arduino.cc/en/Reference/SD)
- [TMRpcm.h](https://github.com/TMRh20/TMRpcm)

Install these via Arduino IDE **Library Manager**.

---

## How It Works

1. **Setup**:
   - Initializes Serial, SD card, and WAV file.
   - Writes a placeholder WAV header with 0 data size.

2. **Recording Loop**:
   - Samples each analog pin and averages the values.
   - Maps analog values (0–1023) to 8-bit PCM (0–255).
   - Writes each byte to the WAV file.
   - Updates visualization buffer for RMS, peak, and ASCII waveform.
   - Tracks zero-crossings for frequency estimation.

3. **Finalization**:
   - Updates WAV header with correct data size.
   - Plays back the recorded file via TMRpcm.

---

## Visualization

- **ASCII waveform**: `+` for positive samples, `-` for negative samples.
- **RMS**: Represents the average power of the signal.
- **Peak**: Maximum amplitude in the buffer.
- **Estimated frequency**: Calculated using zero-crossings.

Example Serial Output:

```
RMS: 45.2  Peak: 78  Wave: ++--++--+--++--+--++
Est. freq: 120.3 Hz
```

---

## Configuration

- `SAMPLE_RATE` — Sampling rate in Hz (default 4000 Hz).
- `RECORD_SECONDS` — Duration of recording.
- `VIZ_WIDTH` — Width of ASCII waveform (number of samples).
- `VIZ_INTERVAL_MS` — How often to print waveform to Serial.
- `FREQ_REPORT_MS` — How often to report estimated frequency.

---

## Notes

- WAV file format: 8-bit PCM, mono.
- Keep `SAMPLE_RATE` low (e.g., 4kHz) to prevent SD write issues.
- Analog inputs can be any signal source, e.g., 555 timer, sensors, or potentiometers.
- Ensure speaker and TMRpcm volume are set properly to avoid distortion.
- Don't use this as it won't work for you guys....
---