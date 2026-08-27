#pragma once

#include <Arduino.h>

class AudioRecorder {
public:
    AudioRecorder(
        int sckPin,
        int wsPin,
        int sdPin
    );

    bool init();

    bool record(uint32_t seconds);

    bool save(const char* path);

    void clear();

    size_t size() const;
    const uint8_t* data() const;

private:
    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr uint16_t BITS_PER_SAMPLE = 16;
    static constexpr uint16_t CHANNELS = 1;
    static constexpr size_t WAV_HEADER_SIZE = 44;

    int sckPin;
    int wsPin;
    int sdPin;

    uint8_t* wavBuffer;
    size_t wavSize;

    bool initialized;

    void writeWavHeader(
        uint8_t* buffer,
        uint32_t dataSize
    );
};