#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "Audio.h"

class Audio;

class AudioPlayer {
public:
    AudioPlayer(int doutPin, int bclkPin, int lrcPin);

    bool init();
    bool play(const char* path);
    bool startStream();
    bool pushStream(const uint8_t* data, size_t length);
    void endStream();

    bool isAudioPlaying() const;
    bool isStreamPlaying() const;
    bool isStreamBufferEmpty() const;

    void setVolume(uint8_t volume);

private:
    int doutPin;
    int bclkPin;
    int lrcPin;

    // TTS
    static constexpr size_t PCM_CHUNK_SIZE = 4096;
    static constexpr size_t PCM_QUEUE_LENGTH = 16;

    struct AudioChunk {
        size_t length;
        uint8_t data[PCM_CHUNK_SIZE];
    };

    QueueHandle_t pcmQueue;
    TaskHandle_t pcmTaskHandle;

    volatile bool streamPlaying;
    volatile bool streamEnded;
    volatile bool audioPlaying;

    static void pcmTaskEntry(void* parameter);
    void pcmTask();

    float volumeGain = 0.3f;
    void applyVolume(int16_t* samples, size_t sampleCount);

    bool configureI2S();
};