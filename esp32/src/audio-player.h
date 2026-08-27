#pragma once

#include <Arduino.h>

class Audio;

class AudioPlayer {
public:
    AudioPlayer(int doutPin, int bclkPin, int lrcPin);

    bool init();
    bool play(const char* path);
    void loop();
    void setVolume(uint8_t volume);

private:
    int doutPin;
    int bclkPin;
    int lrcPin;

    Audio* audio;
};