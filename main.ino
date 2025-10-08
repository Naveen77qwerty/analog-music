/*
  multi_input_signal_recorder.ino
  - Samples analog A1–A4
  - Mixes them into one 8-bit PCM waveform
  - Writes WAV to /signal.wav on SD
  - Prints ASCII waveform bars, RMS, peak, and frequency estimate
  - Plays back using TMRpcm
*/

#include <SPI.h>
#include <SD.h>
#include <TMRpcm.h>

TMRpcm audio;
File wavFile;

const int chipSelect = 10; // SD CS
const int analogPins[] = {A1, A2, A3, A4};
const int numPins = 4;
const int speakerPin = 9; // TMRpcm speaker

// Config
#define SAMPLE_RATE 4000 // Hz
#define BITS_PER_SAMPLE 8
#define CHANNELS 1
const unsigned long RECORD_SECONDS = 8;

// Visualization settings
const unsigned int VIZ_WIDTH = 48;
const unsigned long VIZ_INTERVAL_MS = 150;
const unsigned long FREQ_REPORT_MS = 500;

// Wave analyzer buffer
const int ANALYZER_BUFFER = 32;
byte waveBuffer[ANALYZER_BUFFER];
int waveIndex = 0;

// Function declarations
void writeWavHeader(File &file, unsigned long sampleRate, int bitsPerSample, int channels, unsigned long dataSize);
void updateWavHeader(File &file, unsigned long dataSize);

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;

    Serial.println(F("\n=== Multi-Input Signal Recorder ==="));

    if (!SD.begin(chipSelect))
    {
        Serial.println(F("SD init failed!"));
        while (1)
            ;
    }
    Serial.println(F("SD mounted."));

    // Create WAV file
    wavFile = SD.open("signal.wav", FILE_WRITE);
    if (!wavFile)
    {
        Serial.println(F("Failed to open signal.wav"));
        while (1)
            ;
    }
    writeWavHeader(wavFile, SAMPLE_RATE, BITS_PER_SAMPLE, CHANNELS, 0);

    Serial.print(F("Recording "));
    Serial.print(RECORD_SECONDS);
    Serial.println(F(" seconds..."));

    unsigned long sampleIntervalUs = 1000000UL / SAMPLE_RATE;
    unsigned long nextMicros = micros();

    unsigned long startMillis = millis();
    unsigned long dataBytes = 0;

    unsigned long lastVizMillis = millis();
    unsigned long lastFreqReport = millis();
    unsigned long zeroCrossings = 0;
    const int midPoint = 128;
    bool lastAboveMid = false;
    byte lastSampleByte = midPoint;
    unsigned long peakValue = 0;

    // Initialize analyzer buffer
    for (int i = 0; i < ANALYZER_BUFFER; i++)
        waveBuffer[i] = midPoint;

    // Recording loop
    while (millis() - startMillis < (RECORD_SECONDS * 1000UL))
    {
        unsigned long now = micros();
        if ((long)(now - nextMicros) < 0)
            continue;
        nextMicros += sampleIntervalUs;

        // --- Read & mix analog inputs ---
        int mixedSample = 0;
        for (int p = 0; p < numPins; p++)
        {
            int val = analogRead(analogPins[p]);
            mixedSample += val;
        }
        mixedSample /= numPins; // average
        byte audioByte = map(mixedSample, 0, 1023, 0, 255);

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

        // Serial visualization
        if (nowMillis - lastVizMillis >= VIZ_INTERVAL_MS)
        {
            lastVizMillis = nowMillis;

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

            // ASCII waveform
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
    } // end recording

    wavFile.close();

    // Update WAV header
    wavFile = SD.open("signal.wav", FILE_WRITE);
    if (wavFile)
    {
        updateWavHeader(wavFile, dataBytes);
        wavFile.close();
    }

    Serial.print(F("Recording complete. Data bytes = "));
    Serial.println(dataBytes);

    // Playback
    audio.speakerPin = speakerPin;
    audio.setVolume(4);
    Serial.println(F("Playing back recorded signal.wav ..."));
    audio.play("signal.wav");
}

void loop()
{
    // Idle
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
