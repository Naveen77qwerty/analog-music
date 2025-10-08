#include <SD.h>
#include <TMRpcm.h>

TMRpcm audio;
const int chipSelect = 10; // SD card CS pin

void setup()
{
    Serial.begin(9600);
    while (!Serial)
        ; // Wait for Serial to start

    // Initialize SD card
    if (!SD.begin(chipSelect))
    {
        Serial.println("SD initialization failed!");
        return;
    }
    Serial.println("SD ready.");

    // Set speaker pin
    audio.speakerPin = 9; // D9 PWM output

    // Set volume (0-7)
    audio.setVolume(4);

    // Play first WAV file in MP3 folder
    if (SD.exists("/MP3/NC.wav"))
    {
        Serial.println("Playing wav...");
        audio.play("/MP3/Example.wav"); // File must be WAV
    }
    else
    {
        Serial.println("File not found: /MP3/test.wav");
    }
}

void loop()
{
    // Nothing here, playback runs automatically
}