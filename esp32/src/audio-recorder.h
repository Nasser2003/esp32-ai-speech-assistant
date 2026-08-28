#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

// Sensor: INMP441
class AudioRecorder {
public:
    AudioRecorder(
        int sckPin,
        int wsPin,
        int sdPin
    );

    bool init();

    bool startRecording(
        uint32_t timeoutSeconds = DEFAULT_TIMEOUT
    );

    void update();

    bool stopRecording();

    bool save(const char* path);

    void clear();

    bool isRecordingState() const;

    const uint8_t* data() const;
    size_t getSize() const;

private:
    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint16_t BITS_PER_SAMPLE = 16;
    static constexpr uint16_t CHANNELS = 1;
    static constexpr size_t WAV_HEADER_SIZE = 44;
    static constexpr uint32_t DEFAULT_TIMEOUT = 15;

    static constexpr float HP_ALPHA = 0.95f;
    static constexpr size_t RAW_SAMPLE_COUNT = 256;
    static constexpr float TARGET_LEVEL = 0.9f;

    int sckPin;
    int wsPin;
    int sdPin;

    uint8_t* wavMaxBuffer;
    size_t wavMaxSize;

    uint32_t recordingStartTime;
    uint32_t recordingTimeout;

    float hpPrevIn;
    float hpPrevOut;

    int16_t* pcmMaxData;

    size_t maxSampleCount;
    size_t samplesWritten;

    int16_t minSample;
    int16_t maxSample;

    uint64_t sumSquares;
    uint64_t measuredSamples;

    bool initialized;
    bool isRecording;

    void writeWavHeader(
        uint8_t* buffer,
        uint32_t dataSize
    );

    i2s_config_t createConfig();
    i2s_pin_config_t createPinConfig();
};