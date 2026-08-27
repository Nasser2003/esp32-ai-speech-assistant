#include <Arduino.h>
#include "audio-player.h"
#include "audio-recorder.h"

AudioPlayer audioPlayer(39, 42, 3);
// OledScreen128x32 screen(15, 7);

// INMP441
AudioRecorder recorder(18,17,40);

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("Initialisation recorder...");

    if (!recorder.init()) {
        Serial.println("Recorder initialization failed");

        while (true) {
            delay(1000);
        }
    }

    Serial.println("Recording...");

    if (!recorder.record(5)) {
        Serial.println("Recording failed");

        while (true) {
            delay(1000);
        }
    }

    if (!recorder.save("/recording.wav")) {
        Serial.println("Saving failed");

        while (true) {
            delay(1000);
        }
    }

    Serial.println("Recording ready!");

    Serial.println("Initialisation player...");

    if (!audioPlayer.init()) {
        Serial.println("AudioPlayer initialization failed");

        while (true) {
            delay(1000);
        }
    }

    audioPlayer.setVolume(100);

    if (!audioPlayer.play("/recording.wav")) {
        Serial.println("Playback failed");
    }
}

void loop()
{
    audioPlayer.loop();
    vTaskDelay(1);
}