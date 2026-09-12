#include "audio-recorder.h"
#include "i2s-audio-manager.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <cstring> // string operations
#include <cmath> // math functions
#include <climits> // math limit constants
#include <algorithm>

constexpr uint32_t AudioRecorder::SAMPLE_RATE;
constexpr uint16_t AudioRecorder::BITS_PER_SAMPLE;
constexpr uint16_t AudioRecorder::CHANNELS;
constexpr size_t AudioRecorder::WAV_HEADER_SIZE;
constexpr uint32_t AudioRecorder::DEFAULT_TIMEOUT;

AudioRecorder::AudioRecorder(I2SAudioManager& manager)
    : manager(manager),
      wavBuffer(nullptr), // Buffer to store the recorded audio data in WAV format
      wavSize(0), // Size of the recorded audio data in bytes

      recordingStartTime(0),
      recordingTimeout(0),

      hpPrevIn(0.0f),
      hpPrevOut(0.0f),

      pcmData(nullptr),

      maxSampleCount(0),
      samplesWritten(0),

      minSample(INT16_MAX),
      maxSample(INT16_MIN),

      sumSquares(0),
      measuredSamples(0),

      initialized(false),
      isRecording(false),
      chunkCursor(0)
{
    init();
}

bool AudioRecorder::init()
{
    // The I2S driver is now managed by I2SAudioManager.
    // No need to install it here — activateRX() will be called
    // before each recording session.

    initialized = true;

    Serial.println("[AudioRecorder] Initialized (I2S managed by I2SAudioManager)");
    return true;
}

bool AudioRecorder::startRecording(uint32_t timeoutSeconds)
{
    // --------------------------------------------------
    // 1. Check that the recorder is initialized
    // --------------------------------------------------

    if (!initialized) {
        Serial.println("[AudioRecorder] Not initialized");
        return false;
    }

    if (isRecording) {
        Serial.println("[AudioRecorder] Already recording");
        return false;
    }

    clear();

    // --------------------------------------------------
    // 1b. Activate RX mode on the shared I2S port
    // --------------------------------------------------

    if (!manager.activateRX()) {
        Serial.println("[AudioRecorder] Failed to activate I2S RX");
        return false;
    }

    // Clear old data from the I2S DMA buffer.
    i2s_zero_dma_buffer(I2S_PORT);

    // --------------------------------------------------
    // 2. Calculate maximum buffer size
    // --------------------------------------------------

    maxSampleCount =
        static_cast<size_t>(SAMPLE_RATE) *
        timeoutSeconds;

    const size_t pcmMaxSize =
        maxSampleCount *
        sizeof(int16_t);

    wavSize =
        WAV_HEADER_SIZE +
        pcmMaxSize;


    // --------------------------------------------------
    // 3. Allocate maximum WAV buffer in PSRAM
    // --------------------------------------------------

    wavBuffer =
        static_cast<uint8_t*>(
            ps_malloc(wavSize)
        );

    if (wavBuffer == nullptr) {
        Serial.printf(
            "[AudioRecorder] PSRAM allocation failed: %u bytes\n",
            static_cast<unsigned>(wavSize)
        );

        wavSize = 0;

        return false;
    }


    // --------------------------------------------------
    // 4. Prepare PCM data location
    // --------------------------------------------------

    pcmData =
        reinterpret_cast<int16_t*>(
            wavBuffer +
            WAV_HEADER_SIZE
        );


    // --------------------------------------------------
    // 5. Reset recording state
    // --------------------------------------------------

    samplesWritten = 0;

    minSample = INT16_MAX;
    maxSample = INT16_MIN;

    sumSquares = 0;
    measuredSamples = 0;

    hpPrevIn = 0.0f;
    hpPrevOut = 0.0f;

    recordingStartTime = millis();

    recordingTimeout =
        timeoutSeconds * 1000UL;

    isRecording = true;


    Serial.printf(
        "[AudioRecorder] Recording started "
        "(timeout: %lu s)\n",
        timeoutSeconds
    );

    return true;
}

void AudioRecorder::update()
{
    if (!isRecording) {
        return;
    }


    // --------------------------------------------------
    // Check timeout
    // --------------------------------------------------

    if (
        millis() - recordingStartTime >=
        recordingTimeout
    ) {
        Serial.println(
            "[AudioRecorder] Recording timeout"
        );

        stopRecording();

        return;
    }


    // --------------------------------------------------
    // Temporary I2S buffer
    // --------------------------------------------------

    int32_t rawSamples[RAW_SAMPLE_COUNT];

    size_t bytesRead = 0;


    // --------------------------------------------------
    // Read samples from I2S
    // --------------------------------------------------

    const esp_err_t result =
        i2s_read(
            I2S_PORT,
            rawSamples,
            sizeof(rawSamples),
            &bytesRead,
            0
        );

    if (result != ESP_OK) {
        Serial.printf(
            "[AudioRecorder] i2s_read failed: %d\n",
            result
        );

        stopRecording();

        return;
    }


    // No samples available yet.
    if (bytesRead == 0) {
        return;
    }


    // --------------------------------------------------
    // Convert bytes to sample count
    // --------------------------------------------------

    const size_t samplesRead =
        bytesRead /
        sizeof(int32_t);


    // --------------------------------------------------
    // Process samples
    // --------------------------------------------------

    for (
        size_t i = 0;
        i < samplesRead &&
        samplesWritten < maxSampleCount;
        ++i
    ) {

        // INMP441 provides 24 useful bits
        // inside a 32-bit I2S slot.
        //
        // Convert to 16-bit PCM.
        int16_t sample =
            static_cast<int16_t>(
                rawSamples[i] >> 16
            );


        // --------------------------------------------------
        // High-pass filter
        // --------------------------------------------------

        const float hpOut =
            HP_ALPHA *
            (
                hpPrevOut +
                sample -
                hpPrevIn
            );

        hpPrevIn =
            static_cast<float>(sample);

        hpPrevOut = hpOut;

        sample =
            static_cast<int16_t>(hpOut);


        // --------------------------------------------------
        // Preamp gain (boost INMP441 low output)
        // --------------------------------------------------

        int32_t boosted =
            static_cast<int32_t>(sample) * PREAMP_GAIN;

        if (boosted > INT16_MAX) boosted = INT16_MAX;
        if (boosted < INT16_MIN) boosted = INT16_MIN;

        sample = static_cast<int16_t>(boosted);


        // --------------------------------------------------
        // Store sample
        // --------------------------------------------------

        pcmData[samplesWritten++] = sample; // Store the sample in the PCM buffer and increment the sample count


        // --------------------------------------------------
        // Update statistics
        // --------------------------------------------------

        if (sample < minSample) {
            minSample = sample;
        }

        if (sample > maxSample) {
            maxSample = sample;
        }

        sumSquares +=
            static_cast<int64_t>(sample) *
            sample;

        measuredSamples++;
    }


    // --------------------------------------------------
    // Maximum recording duration reached
    // --------------------------------------------------

    if (samplesWritten >= maxSampleCount) {
        Serial.println(
            "[AudioRecorder] Maximum recording size reached"
        );

        stopRecording();
    }
}


const uint8_t* AudioRecorder::fetchRecordedChunk(size_t& size) 
{ // "size_t& size" reference needed to implicitely return a second variable after "uint8_t*"
    const size_t totalSamples = samplesWritten;


    if (chunkCursor >= totalSamples) { // No new audio available at the size of a chunk
        size = 0;
        return nullptr;
    }

    const size_t remainingSamples = totalSamples - chunkCursor;

    // While recording, wait until a complete chunk is available.
    if (
        isRecording &&
        remainingSamples < RECORDED_SAMPLE_CHUNK_SIZE
    ) {
        size = 0;
        return nullptr;
    }

    const size_t chunkStart = chunkCursor;

    const size_t chunkSamples =
        std::min(
            remainingSamples,
            RECORDED_SAMPLE_CHUNK_SIZE
        );

    chunkCursor += chunkSamples;

    // Convert the number of samples to bytes.
    size = chunkSamples * sizeof(int16_t);

    return reinterpret_cast<const uint8_t*>(
        pcmData + chunkStart
    );
}

bool AudioRecorder::stopRecording()
{
    if (!isRecording) {
        return false;
    }

    isRecording = false;

    // --------------------------------------------------
    // Trim tail samples (remove button click artifact)
    // --------------------------------------------------

    if (samplesWritten > TRIM_TAIL_SAMPLES) {
        samplesWritten -= TRIM_TAIL_SAMPLES;
        Serial.printf(
            "[AudioRecorder] Trimmed last %u samples (~%ums)\n",
            static_cast<unsigned>(TRIM_TAIL_SAMPLES),
            static_cast<unsigned>(TRIM_TAIL_SAMPLES * 1000 / SAMPLE_RATE)
        );
    }

    // --------------------------------------------------
    // Calculate actual recording size
    // --------------------------------------------------

    const size_t pcmSize = samplesWritten * sizeof(int16_t);


    // --------------------------------------------------
    // Build amplitude histogram
    // --------------------------------------------------

    uint32_t histogram[NORMALIZATION_BINS] = {};

    for (size_t i = 0; i < samplesWritten; ++i) {

        const int32_t amplitude =
            std::abs(
                static_cast<int32_t>(pcmData[i])
            );

        const size_t bin =
            static_cast<size_t>(
                (static_cast<uint64_t>(amplitude) *
                 NORMALIZATION_BINS) /
                32768
            );

        const size_t safeBin =
            std::min(
                bin,
                NORMALIZATION_BINS - 1
            );

        histogram[safeBin]++;
    }


    // --------------------------------------------------
    // Find percentile amplitude
    // --------------------------------------------------

    const size_t targetSamples =
        static_cast<size_t>(
            samplesWritten *
            NORMALIZATION_PERCENTILE
        );

    size_t accumulatedSamples = 0;
    int32_t normalizationPeak = 0;

    for (size_t i = 0; i < NORMALIZATION_BINS; ++i) {

        accumulatedSamples += histogram[i];

        if (accumulatedSamples >= targetSamples) {

            normalizationPeak =
                static_cast<int32_t>(
                    ((i + 1) * 32768) /
                    NORMALIZATION_BINS
                );

            break;
        }
    }

    // --------------------------------------------------
    // Normalize volume
    // --------------------------------------------------

    if (normalizationPeak > 0) {

        const float gain =
            (INT16_MAX * TARGET_LEVEL) /
            static_cast<float>(normalizationPeak);

        for (size_t i = 0; i < samplesWritten; ++i) {

            int32_t amplified =
                static_cast<int32_t>(
                    pcmData[i] * gain
                );

            // Prevent overflow.
            if (amplified > INT16_MAX) {
                amplified = INT16_MAX;
            }

            if (amplified < INT16_MIN) {
                amplified = INT16_MIN;
            }

            pcmData[i] =
                static_cast<int16_t>(amplified);
        }

        Serial.printf(
            "[AudioRecorder] Normalized, gain x%.2f, "
            "percentile peak: %ld\n",
            gain,
            normalizationPeak
        );
    }

    // --------------------------------------------------
    // Calculate actual WAV size
    // --------------------------------------------------

    wavSize = WAV_HEADER_SIZE + pcmSize;

    // --------------------------------------------------
    // Create WAV header
    // --------------------------------------------------

    writeWavHeader(
        wavBuffer,
        static_cast<uint32_t>(pcmSize)
    );

    Serial.printf(
        "[AudioRecorder] Recording finished: %u bytes\n",
        static_cast<unsigned>(wavSize)
    );

    return true;
}

bool AudioRecorder::save(const char* path)
{
    if (wavBuffer == nullptr || wavSize == 0) {
        Serial.println(
            "[AudioRecorder] No recording to save"
        );

        return false;
    }

    if (!LittleFS.begin(false)) {
        Serial.println(
            "[AudioRecorder] LittleFS initialization failed"
        );

        return false;
    }

    File file = LittleFS.open(
        path,
        "w"
    );

    if (!file) {
        Serial.printf(
            "[AudioRecorder] Cannot open %s\n",
            path
        );

        return false;
    }

    const size_t written =
        file.write(
            wavBuffer,
            wavSize
        );

    file.close();

    if (written != wavSize) {
        Serial.printf(
            "[AudioRecorder] Write error: %u/%u bytes\n",
            static_cast<unsigned>(written),
            static_cast<unsigned>(wavSize)
        );

        return false;
    }

    Serial.printf(
        "[AudioRecorder] Saved: %s (%u bytes)\n",
        path,
        static_cast<unsigned>(wavSize)
    );

    return true;
}

void AudioRecorder::clear()
{
    if (isRecording) {
        Serial.println(
            "[AudioRecorder] Cannot clear while recording"
        );

        return;
    }


    if (wavBuffer != nullptr) {
        free(wavBuffer);

        wavBuffer = nullptr;
    }

    pcmData = nullptr;

    wavSize = 0;
    maxSampleCount = 0;
    samplesWritten = 0;
    chunkCursor = 0;
}

size_t AudioRecorder::getSize() const
{
    return wavSize;
}

bool AudioRecorder::isRecordingState() const
{
    return isRecording;
}

const uint8_t* AudioRecorder::data() const
{
    return wavBuffer;
}

// PRIVATE METHODS

void AudioRecorder::writeWavHeader(uint8_t* buffer, uint32_t dataSize)
{
    const uint32_t byteRate = SAMPLE_RATE * CHANNELS * BITS_PER_SAMPLE /8;
    const uint16_t blockAlign = CHANNELS * BITS_PER_SAMPLE / 8;
    const uint32_t chunkSize = 36 + dataSize;
    const uint32_t fmtSize = 16;
    const uint16_t audioFormat = 1;

    // RIFF
    std::memcpy(buffer + 0, "RIFF", 4);
    std::memcpy(buffer + 4, &chunkSize, 4);

    // WAVE
    std::memcpy(buffer + 8, "WAVE", 4);

    // fmt
    std::memcpy(buffer + 12, "fmt ", 4);
    std::memcpy(buffer + 16, &fmtSize, 4);
    std::memcpy(buffer + 20, &audioFormat, 2);
    std::memcpy(buffer + 22, &CHANNELS, 2);
    std::memcpy(buffer + 24, &SAMPLE_RATE, 4);
    std::memcpy(buffer + 28, &byteRate, 4);
    std::memcpy(buffer + 32, &blockAlign, 2);
    std::memcpy(buffer + 34, &BITS_PER_SAMPLE, 2);

    // data
    std::memcpy(buffer + 36, "data", 4);
    std::memcpy(buffer + 40, &dataSize, 4);
}