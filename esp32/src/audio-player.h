#pragma once

#include <Arduino.h>
#include <LittleFS.h>

class I2SAudioManager;

class AudioPlayer {
public:
    AudioPlayer(I2SAudioManager& manager);

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
    // I2S audio manager reference
    I2SAudioManager& manager;

    // TTS
    // Chunk size for the PCM FreeRTOS queue.
    // Kept small (512 B) so the queue footprint stays ~20 KB, leaving enough
    // contiguous heap (~22 KB+) for ArduinoWebsockets to buffer a binary TTS frame.
    static constexpr size_t PCM_CHUNK_SIZE = 512;
    // 40 × 516 B ≈ 20 KB queue — ~640 ms of 16-kHz/16-bit/mono buffering.
    static constexpr size_t PCM_QUEUE_LENGTH = 40;

    struct AudioChunk {
        size_t length;
        int16_t data[PCM_CHUNK_SIZE / sizeof(int16_t)];
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
};