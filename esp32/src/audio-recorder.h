#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

class I2SAudioManager;

// Sensor: INMP441
class AudioRecorder {
public:
    AudioRecorder(I2SAudioManager& manager);

    bool init();

    bool startRecording(
        uint32_t timeoutSeconds = DEFAULT_TIMEOUT
    );

    void update();

    bool stopRecording();

    const uint8_t* fetchRecordedChunk(size_t& size);

    void clear();

    bool isRecordingState() const;

    size_t getSize() const; // bytes of recorded PCM data from startRecording()

private:
    // Audio format
    static constexpr uint32_t SAMPLE_RATE = 16000;           // Number of audio samples captured per second (16 kHz)
    static constexpr uint16_t BITS_PER_SAMPLE = 16;          // Bit depth of the final PCM audio data
    static constexpr uint16_t CHANNELS = 1;                  // Number of audio channels (mono)
    static constexpr uint32_t DEFAULT_TIMEOUT = 15;          // Maximum recording duration in seconds by default

    // Audio processing
    static constexpr float HP_ALPHA = 0.95f;                 // High-pass filter coefficient used to reduce low-frequency noise
    static constexpr size_t RAW_SAMPLE_COUNT = 256;          // Number of raw 32-bit samples read from I2S at once
    static constexpr float TARGET_LEVEL = 0.9f;              // Target peak level after audio normalization (90% of full scale)
    static constexpr size_t NORMALIZATION_BINS = 256;        // Number of amplitude ranges used to build the normalization histogram
    static constexpr float NORMALIZATION_PERCENTILE = 0.995f; // Percentile used to ignore short abnormal peaks during normalization
    static constexpr size_t RECORDED_SAMPLE_CHUNK_SIZE = 8000; // Maximum number of PCM samples returned per chunk (0.5 s at 16 kHz)
    static constexpr int32_t PREAMP_GAIN = 4;                // Real-time gain applied to each sample to boost INMP441 low output
    static constexpr size_t TRIM_TAIL_SAMPLES = 3200;        // Samples to discard at end of recording (~200ms at 16 kHz, removes button click)

    // I2S port (shared via manager)
    static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;

    // I2S audio manager reference
    I2SAudioManager& manager;

    // Recording timing
    uint32_t recordingStartTime;                   // Timestamp when the current recording started
    uint32_t recordingTimeout;                     // Maximum recording duration in milliseconds

    // High-pass filter state
    float hpPrevIn;                                // Previous input sample used by the high-pass filter
    float hpPrevOut;                               // Previous output sample used by the high-pass filter

    // Double buffer : replaces old buffer stored in PSRAM
    int16_t chunkBuf[2][RECORDED_SAMPLE_CHUNK_SIZE];
    size_t writePos;    // write position in the active buffer
    int activeBuf;      // 0 or 1 : buffer in fill mode
    int readyBuf;       // -1 = nothing ready, else 0/1 : buffer ready to be fetched
    size_t readySize;   // valid samples in readyBuf

    size_t samplesWritten; // total captured since startRecording()

    // Recorder state
    bool initialized;                              // True when the I2S microphone has been successfully initialized
    bool isRecording;                              // True while audio is currently being recorded
};