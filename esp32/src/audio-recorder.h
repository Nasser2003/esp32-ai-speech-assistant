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

    bool save(const char* path);

    void clear();

    bool isRecordingState() const;

    const uint8_t* data() const;
    size_t getSize() const;

private:
    // Audio format
    static constexpr uint32_t SAMPLE_RATE = 16000;           // Number of audio samples captured per second (16 kHz)
    static constexpr uint16_t BITS_PER_SAMPLE = 16;          // Bit depth of the final PCM audio data
    static constexpr uint16_t CHANNELS = 1;                  // Number of audio channels (mono)
    static constexpr size_t WAV_HEADER_SIZE = 44;            // Size of the standard PCM WAV header in bytes
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

    // WAV buffer
    uint8_t* wavBuffer;                            // Buffer containing both the WAV header and PCM audio data
    size_t wavSize;                                // Actual size of the recorded WAV data in bytes

    // Recording timing
    uint32_t recordingStartTime;                   // Timestamp when the current recording started
    uint32_t recordingTimeout;                     // Maximum recording duration in milliseconds

    // High-pass filter state
    float hpPrevIn;                                // Previous input sample used by the high-pass filter
    float hpPrevOut;                               // Previous output sample used by the high-pass filter

    // PCM audio data
    int16_t* pcmData;                              // Pointer to the PCM section inside wavBuffer, after the 44-byte WAV header

    size_t maxSampleCount;                         // Maximum number of PCM samples that can be stored
    size_t samplesWritten;                         // Number of PCM samples currently stored

    // Audio statistics
    int16_t minSample;                             // Lowest sample value measured during recording
    int16_t maxSample;                             // Highest sample value measured during recording

    uint64_t sumSquares;                           // Sum of squared sample values, used to calculate RMS
    uint64_t measuredSamples;                      // Number of samples included in the audio statistics

    // Recorder state
    bool initialized;                              // True when the I2S microphone has been successfully initialized
    bool isRecording;                              // True while audio is currently being recorded

    // WAV generation
    void writeWavHeader(
        uint8_t* buffer,
        uint32_t dataSize
    );

    // audio chunking
    int chunkCursor;                                // Current position in the PCM data for fetching recorded chunks
};