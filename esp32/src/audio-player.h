#pragma once

#include <Arduino.h>
#include <LittleFS.h>

class I2SAudioManager;

class AudioPlayer {
public:
    AudioPlayer(I2SAudioManager& manager);

    bool init();
    bool play(const char* path);
    void stop();
    bool startStream();
    bool pushStream(const uint8_t* data, size_t length);
    void endStream();

    bool isPlaying() const;
    bool isAudioPlaying() const;
    bool isStreamPlaying() const;
    bool isStreamBufferEmpty() const;
    // Returns true once the DMA has had enough time to output the last PCM chunk.
    // Call after isStreamBufferEmpty() to avoid cutting off the audio tail.
    bool isStreamDrained() const;

    void setVolume(uint8_t volume);
    uint8_t getVolume() const;

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

    // Producteur "fichier" : partage pcmQueue/pcmTask avec le TTS.
    static constexpr size_t MAX_PATH_LEN = 64;
    char filePath[MAX_PATH_LEN];
    TaskHandle_t filePlaybackTaskHandle;

    volatile bool streamPlaying;
    volatile bool streamEnded;
    volatile bool audioPlaying;
    volatile bool wavPlaying;   // true tant que la tâche fichier pousse des chunks
    volatile bool stopRequested;
    volatile bool ttsBusy;
    volatile bool isDraining;   // true after sentinel until DMA_DRAIN_MS have elapsed
    volatile uint32_t drainStartMs;
    // DMA drain time: dma_buf_count(16) × dma_buf_len(256) samples @ 16 kHz → ~256 ms, use 350 ms margin
    static constexpr uint32_t DMA_DRAIN_MS = 350;

    static void pcmTaskEntry(void* parameter);
    void pcmTask();

    static void filePlaybackTaskEntry(void* parameter);
    void filePlaybackTask();

    float volumeGain = 0.3f;
    void applyVolume(int16_t* samples, size_t sampleCount);
};