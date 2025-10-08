/*
  signal_recorder_with_analyzer.ino
  - Writes 8-bit unsigned mono WAV to /signal.wav on SD
  - Prints ASCII waveform bars, RMS, peak, and frequency estimate on Serial
  - Plays back using TMRpcm (speaker on D9)
*/

#include <SPI.h>
#include <SD.h>
#include <TMRpcm.h>

TMRpcm audio;
File wavFile;

const int chipSelect = 10; // SD CS (change if different)
const int analogPin = A0;  // Signal input (555 OUT via divider)
const int speakerPin = 9;  // TMRpcm speaker pin

// Config
#define SAMPLE_RATE 4000 // Hz
#define BITS_PER_SAMPLE 8
#define CHANNELS 1
const unsigned long RECORD_SECONDS = 8; // record duration

// Visualization settings
const unsigned int VIZ_WIDTH = 48;         // number of chars in bar graph
const unsigned long VIZ_INTERVAL_MS = 150; // update serial viz every ~150ms
const unsigned long FREQ_REPORT_MS = 500;  // print estimated frequency every 500ms

// Wave analyzer settings
const int ANALYZER_BUFFER = 32; // last N samples for RMS & peak
byte waveBuffer[ANALYZER_BUFFER];
int waveIndex = 0;

// Function declarations
void writeWavHeader(File &file, unsigned long sampleRate, int bitsPerSample, int channels, unsigned long dataSize);
void updateWavHeader(File &file, unsigned long dataSize);

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ; // wait for serial

    Serial.println(F("\n=== Signal Recorder with Analyzer ==="));

    if (!SD.begin(chipSelect))
    {
        Serial.println(F("SD init failed! Check wiring and CS pin."));
        while (1)
            ;
    }
    Serial.println(F("SD mounted."));

    // Create or overwrite the WAV file
    wavFile = SD.open("signal.wav", FILE_WRITE);
    if (!wavFile)
    {
        Serial.println(F("Failed to open signal.wav for writing"));
        while (1)
            ;
    }

    // Write placeholder WAV header (zeros for sizes)
    writeWavHeader(wavFile, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS, 0);

    Serial.print(F("Recording "));
    Serial.print(RECORD_SECONDS);
    Serial.println(F(" seconds..."));

    // Sampling timing
    unsigned long sampleIntervalUs = 1000000UL / SAMPLE_RATE;
    unsigned long nextMicros = micros();

    unsigned long startMillis = millis();
    unsigned long dataBytes = 0;

    // For visualization & freq estimation
    unsigned long lastVizMillis = millis();
    unsigned long lastFreqReport = millis();
    unsigned long zeroCrossings = 0;
    bool lastAboveMid = false;
    const int midPoint = 128; // for 8-bit unsigned PCM mid line
    byte lastSampleByte = midPoint;
    unsigned long peakValue = 0;

    // Initialize wave buffer
    for (int i = 0; i < ANALYZER_BUFFER; i++)
        waveBuffer[i] = midPoint;

    // Recording loop
    while (millis() - startMillis < (RECORD_SECONDS * 1000UL))
    {
        // wait until next sample time
        unsigned long now = micros();
        if ((long)(now - nextMicros) < 0)
            continue;
        nextMicros += sampleIntervalUs;

        // Read analog (0-1023)
        int raw = analogRead(analogPin);

        // Map to 8-bit unsigned PCM (0..255)
        byte audioByte = map(raw, 0, 1023, 0, 255);

        // write sample to SD
        wavFile.write(&audioByte, 1);
        dataBytes++;

        // zero-crossing detection
        bool aboveMid = (audioByte >= midPoint);
        if (aboveMid && !lastAboveMid)
            zeroCrossings++;
        lastAboveMid = aboveMid;
        lastSampleByte = audioByte;

        // ---- Wave analyzer ----
        waveBuffer[waveIndex] = audioByte;
        waveIndex = (waveIndex + 1) % ANALYZER_BUFFER;

        unsigned long nowMillis = millis();

        // Serial visualization (waveform, RMS, peak)
        if (nowMillis - lastVizMillis >= VIZ_INTERVAL_MS)
        {
            lastVizMillis = nowMillis;

            // Compute RMS & peak
            unsigned long sumSq = 0;
            peakValue = 0;
            for (int i = 0; i < ANALYZER_BUFFER; i++)
            {
                int val = (int)waveBuffer[i] - midPoint;
                sumSq += val * val;
                if (abs(val) > peakValue)
                    peakValue = abs(val);
            }
            float rms = sqrt(sumSq / (float)ANALYZER_BUFFER);

            // ASCII waveform summary
            String line = "";
            for (int i = 0; i < ANALYZER_BUFFER; i++)
            {
                int val = (int)waveBuffer[i] - midPoint;
                line += (val >= 0) ? '+' : '-';
            }

            Serial.print("RMS:");
            Serial.print(rms, 1);
            Serial.print("  Peak:");
            Serial.print(peakValue);
            Serial.print("  Wave:");
            Serial.println(line);
        }

        // Frequency reporting
        if (nowMillis - lastFreqReport >= FREQ_REPORT_MS)
        {
            lastFreqReport = nowMillis;
            float elapsedSec = (nowMillis - startMillis) / 1000.0f;
            float freq = 0.0f;
            if (elapsedSec > 0.05)
                freq = zeroCrossings / elapsedSec;
            Serial.print(F("Est. freq: "));
            Serial.print(freq, 1);
            Serial.println(F(" Hz"));
        }
    } // end recording loop

    wavFile.close();

    // Update WAV header with actual data size
    wavFile = SD.open("signal.wav", FILE_WRITE);
    if (!wavFile)
    {
        Serial.println(F("Unable to reopen signal.wav to update header."));
    }
    else
    {
        updateWavHeader(wavFile, dataBytes);
        wavFile.close();
    }

    Serial.print(F("Recording complete. Data bytes = "));
    Serial.println(dataBytes);

    // Playback via TMRpcm
    audio.speakerPin = speakerPin;
    audio.setVolume(4);
    Serial.println(F("Playing back recorded signal.wav ..."));
    audio.play("signal.wav");
}

void loop()
{
    // Idle. Playback runs in library ISR.
}

/*** WAV header helpers ***/
void writeWavHeader(File &file, unsigned long sampleRate, int bitsPerSample, int channels, unsigned long dataSize)
{
    unsigned long byteRate = sampleRate * channels * bitsPerSample / 8;
    unsigned long chunkSize = 36 + dataSize;
    unsigned long subchunk2Size = dataSize;

    file.seek(0);
    file.write("RIFF", 4);
    file.write((byte *)&chunkSize, 4);
    file.write("WAVEfmt ", 8);
    unsigned long subchunk1Size = 16;
    file.write((byte *)&subchunk1Size, 4);
    unsigned short audioFormat = 1; // PCM
    file.write((byte *)&audioFormat, 2);
    unsigned short ch = channels;
    file.write((byte *)&ch, 2);
    file.write((byte *)&sampleRate, 4);
    file.write((byte *)&byteRate, 4);
    unsigned short blockAlign = channels * bitsPerSample / 8;
    file.write((byte *)&blockAlign, 2);
    unsigned short bps = bitsPerSample;
    file.write((byte *)&bps, 2);
    file.write("data", 4);
    file.write((byte *)&subchunk2Size, 4);
}

void updateWavHeader(File &file, unsigned long dataSize)
{
    unsigned long chunkSize = 36 + dataSize;
    unsigned long subchunk2Size = dataSize;

    file.seek(4);
    file.write((byte *)&chunkSize, 4);
    file.seek(40);
    file.write((byte *)&subchunk2Size, 4);
}
