#include "audio-player.h"

#include <LittleFS.h>
#include "Audio.h"

AudioPlayer::AudioPlayer(int doutPin, int bclkPin, int lrcPin)
    : doutPin(doutPin),
      bclkPin(bclkPin),
      lrcPin(lrcPin),
      audio(new Audio())
{
}

bool AudioPlayer::init() {
    Serial.println("Initialisation LittleFS...");

    if (!LittleFS.begin(false)) {
        Serial.println("Erreur : LittleFS");
        return false;
    }

    Serial.printf(
        "Flash: %u MB\n",
        ESP.getFlashChipSize() / (1024 * 1024)
    );

    Serial.printf(
        "PSRAM: %u MB\n",
        ESP.getPsramSize() / (1024 * 1024)
    );

    audio->setPinout(bclkPin, lrcPin, doutPin);
    audio->setVolume(10);

    Serial.println("AudioPlayer prêt");

    return true;
}

bool AudioPlayer::play(const char* path) {
    File file = LittleFS.open(path, "r");

    if (!file) {
        Serial.printf("WAV introuvable : %s\n", path);
        return false;
    }

    Serial.printf(
        "Lecture : %s (%u octets)\n",
        path,
        (unsigned)file.size()
    );

    file.close();

    if (!audio->connecttoFS(LittleFS, path)) {
        Serial.println("Erreur ouverture audio");
        return false;
    }

    return true;
}

void AudioPlayer::setVolume(uint8_t volume) {
    audio->setVolume(volume);
}

void AudioPlayer::loop() {
    audio->loop();
}


// EXAMPLE
// constexpr int I2S_DOUT = 39;
// constexpr int I2S_BCLK = 42;
// constexpr int I2S_LRC = 3;

// AudioPlayer audioPlayer(
//     I2S_DOUT,
//     I2S_BCLK,
//     I2S_LRC
// );

// void setup() {
//     Serial.begin(115200);
//     delay(1000);

//     if (!audioPlayer.init()) {
//         Serial.println("AudioPlayer initialization failed");

//         while (true) {
//             delay(1000);
//         }
//     }
    
//     audioPlayer.setVolume(10);

//     audioPlayer.play("/the_perfect_christmas.wav");
// }

// void loop() {
//     audioPlayer.loop();
//     vTaskDelay(1);
// }