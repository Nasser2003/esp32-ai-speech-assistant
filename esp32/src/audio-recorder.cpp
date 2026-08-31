#include "audio-recorder.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <cstring> // string operations
#include <cmath> // math functions
#include <climits> // math limit constants
#include <algorithm>

constexpr uint32_t AudioRecorder::SAMPLE_RATE;
constexpr uint16_t AudioRecorder::BITS_PER_SAMPLE;
constexpr uint16_t AudioRecorder::CHANNELS;
constexpr size_t AudioRecorder::WAV_HEADER_SIZE;
constexpr uint32_t AudioRecorder::DEFAULT_TIMEOUT;

namespace {
constexpr i2s_port_t I2S_MIC_PORT = I2S_NUM_1;
}

AudioRecorder::AudioRecorder(
    int sckPin,
    int wsPin,
    int sdPin
)
    : sckPin(sckPin), // Serial Clock (SCK) used to synchronize data transmission between the ESP32 and the INMP441 microphone
      wsPin(wsPin), // Word Select (WS) used to indicate the start of a new audio sample and to select the left or right channel for stereo audio
      sdPin(sdPin), // Serial Data (SD) used to transmit the audio data from the INMP441 microphone to the ESP32
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
    // Define the I2S configuration for the INMP441 microphone
    i2s_config_t config = createConfig();

    // Activate/initialize the I2S microphone driver so my program can use it, but if fails return false
    const esp_err_t result = i2s_driver_install(I2S_MIC_PORT, &config, 0, nullptr);
    if (result != ESP_OK) {
        Serial.printf("[AudioRecorder] i2s_driver_install failed: %d\n", result);
        return false;
    }

    // Configure the pins for the I2S microphone, but if fails deactivate/free the driver and return false
    i2s_pin_config_t pins = createPinConfig();

    const esp_err_t pinResult = i2s_set_pin(I2S_MIC_PORT, &pins); // Set the I2S pins for the microphone
    if (pinResult != ESP_OK) {
        Serial.printf("[AudioRecorder] i2s_set_pin failed: %d\n", pinResult);
        i2s_driver_uninstall(I2S_MIC_PORT);
        return false;
    }

    i2s_zero_dma_buffer(I2S_MIC_PORT); // Clear the DMA buffer to avoid any garbage data being read from the microphone
    initialized = true; // Set the initialized flag to true to indicate that the INMP441 has been successfully initialized

    Serial.println("[AudioRecorder] INMP441 initialized");
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

    // Clear old data from the I2S DMA buffer.
    i2s_zero_dma_buffer(I2S_MIC_PORT);

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
            I2S_MIC_PORT,
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

i2s_config_t AudioRecorder::createConfig() {
    i2s_config_t config = {}; // Initialize the config structure to zero 
    
    // Configure the I2S peripheral: 
    //  - I2S_MODE_MASTER -> esp32 will generate the clock for the I2S communication
    //  - I2S_MODE_RX -> esp32 will receive audio data from the INMP441 microphone
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);

    // Configure the I2S peripheral
    config.sample_rate = SAMPLE_RATE; // number of measurements per second (ex: 16 kHz)
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT; // Size of each audio sample (ex: 32 bits)
    config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT; // INMP441 is mono, left channel only
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S; // How to interpret signals sent by the INMP441 microphone
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1; // Interrupt level 1, the lowest priority, to avoid interfering with other tasks
    // DMA (Direct Memory Access) is used to transfer data between the I2S peripheral and memory without CPU intervention => faster
    //   => when 1st buffer is full, DMA will automatically switch to the 2nd buffer and trigger an interrupt to notify the CPU 
    //      that the 1st buffer is ready to be processed. if the last buffer is full, DMA will switch to the 1st buffer.
    config.dma_buf_count = 8; // 8 DMA buffers to store audio data before processing
    config.dma_buf_len = 256; // Number of audio samples stored per DMA buffer
    config.use_apll = false; // Use the APLL clock for better accuracy, but it is not necessary for audio recording
    config.tx_desc_auto_clear = false; // Automatically clear the TX descriptor if there is an underflow condition
    config.fixed_mclk = 0; // The MCLK pin is not used, so it is set to 0

    return config;
}

i2s_pin_config_t AudioRecorder::createPinConfig() {
    i2s_pin_config_t pinConfig = {}; // Initialize the pinConfig structure to zero

    // Configure the I2S pins
    pinConfig.bck_io_num = sckPin; // Serial Clock (SCK) pin
    pinConfig.ws_io_num = wsPin; // Word Select (WS) pin
    pinConfig.data_out_num = I2S_PIN_NO_CHANGE; // No data output pin, as we are only recording audio
    pinConfig.data_in_num = sdPin; // Serial Data (SD) pin

    return pinConfig;
}

// EXAMPLE
// void setup()
// {
//     Serial.begin(115200);
//     delay(1000);

//     Serial.println("Initialisation recorder...");

//     if (!recorder.init()) {
//         Serial.println("Recorder initialization failed");

//         while (true) {
//             delay(1000);
//         }
//     }

//     Serial.println("Recording...");

//     if (!recorder.startRecording(5)) {
//         Serial.println("Recording failed");

//         while (true) {
//             delay(1000);
//         }
//     }

//     if (!recorder.save("/recording.wav")) {
//         Serial.println("Saving failed");

//         while (true) {
//             delay(1000);
//         }
//     }

//     Serial.println("Recording ready!");
// }