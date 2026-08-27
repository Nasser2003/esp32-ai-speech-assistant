#include "audio-recorder.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <cstring>
#include <cmath>
#include <climits>

constexpr uint32_t AudioRecorder::SAMPLE_RATE;
constexpr uint16_t AudioRecorder::BITS_PER_SAMPLE;
constexpr uint16_t AudioRecorder::CHANNELS;

namespace {

constexpr i2s_port_t I2S_MIC_PORT = I2S_NUM_1;

}

AudioRecorder::AudioRecorder(
    int sckPin,
    int wsPin,
    int sdPin
)
    : sckPin(sckPin),
      wsPin(wsPin),
      sdPin(sdPin),
      wavBuffer(nullptr),
      wavSize(0),
      initialized(false)
{
}

bool AudioRecorder::init()
{
    i2s_config_t config = {};

    config.mode = static_cast<i2s_mode_t>(
        I2S_MODE_MASTER |
        I2S_MODE_RX
    );

    // INMP441 → acquisition à 16 kHz
    config.sample_rate = SAMPLE_RATE;

    /*
     * L'INMP441 produit des échantillons dans
     * des slots I²S de 32 bits.
     */
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;

    /*
     * L/R = GND sur l'INMP441
     * → canal gauche.
     */
    config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;

    config.communication_format = I2S_COMM_FORMAT_I2S;

    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;

    config.dma_buf_count = 8;
    config.dma_buf_len = 256;

    config.use_apll = false;
    config.tx_desc_auto_clear = false;
    config.fixed_mclk = 0;

    const esp_err_t result = i2s_driver_install(
        I2S_MIC_PORT,
        &config,
        0,
        nullptr
    );

    if (result != ESP_OK) {
        Serial.printf(
            "[AudioRecorder] i2s_driver_install failed: %d\n",
            result
        );

        return false;
    }

    i2s_pin_config_t pins = {};

    pins.bck_io_num = sckPin;
    pins.ws_io_num = wsPin;

    // Pas de sortie audio.
    pins.data_out_num = I2S_PIN_NO_CHANGE;

    // SD de l'INMP441.
    pins.data_in_num = sdPin;

    const esp_err_t pinResult = i2s_set_pin(
        I2S_MIC_PORT,
        &pins
    );

    if (pinResult != ESP_OK) {
        Serial.printf(
            "[AudioRecorder] i2s_set_pin failed: %d\n",
            pinResult
        );

        i2s_driver_uninstall(I2S_MIC_PORT);

        return false;
    }

    i2s_zero_dma_buffer(I2S_MIC_PORT);

    initialized = true;

    Serial.println("[AudioRecorder] INMP441 initialized");

    return true;
}

bool AudioRecorder::record(uint32_t seconds)
{
    if (!initialized) {
        Serial.println(
            "[AudioRecorder] Not initialized"
        );

        return false;
    }

    clear();

    const size_t sampleCount =
        static_cast<size_t>(SAMPLE_RATE) * seconds;

    const size_t pcmSize =
        sampleCount * sizeof(int16_t);

    wavSize =
        WAV_HEADER_SIZE + pcmSize;

    /*
     * L'enregistrement est placé en PSRAM.
     *
     * 5 secondes :
     * 16000 × 2 × 5 = 160000 octets
     */
    wavBuffer = static_cast<uint8_t*>(
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

    Serial.printf(
        "[AudioRecorder] Recording %lu seconds...\n",
        seconds
    );

    int16_t* pcmData =
        reinterpret_cast<int16_t*>(
            wavBuffer + WAV_HEADER_SIZE
        );

    constexpr size_t RAW_SAMPLE_COUNT = 256;

    int32_t rawSamples[RAW_SAMPLE_COUNT];

    size_t samplesWritten = 0;

    int16_t minSample = INT16_MAX;
    int16_t maxSample = INT16_MIN;
    uint64_t sumSquares = 0;
    size_t measuredSamples = 0;
    
    // noice reduction
    static float hpPrevIn = 0.0f;
    static float hpPrevOut = 0.0f;
    // constexpr float HP_ALPHA = 0.995f;
    constexpr float HP_ALPHA = 0.95f;
    constexpr int16_t NOISE_GATE_THRESHOLD = 200;

    while (samplesWritten < sampleCount) {

        size_t bytesRead = 0;

        const esp_err_t result = i2s_read(
            I2S_MIC_PORT,
            rawSamples,
            sizeof(rawSamples),
            &bytesRead,
            portMAX_DELAY
        );

        if (result != ESP_OK) {
            Serial.printf(
                "[AudioRecorder] i2s_read failed: %d\n",
                result
            );

            clear();

            return false;
        }

        const size_t samplesRead =
            bytesRead / sizeof(int32_t);

        for (
            size_t i = 0;
            i < samplesRead &&
            samplesWritten < sampleCount;
            ++i
        ) {
            /*
             * L'INMP441 fournit 24 bits utiles
             * dans un slot 32 bits.
             *
             * On réduit vers du PCM 16 bits.
             */
            int16_t sample = static_cast<int16_t>(rawSamples[i] >> 16);
            
            // if (std::abs(static_cast<int>(sample)) < NOISE_GATE_THRESHOLD) {
            //     sample = 0;
            // }
            
            // --- filtre passe-haut, appliqué ici ---
            float hpOut = HP_ALPHA * (hpPrevOut + sample - hpPrevIn);
            hpPrevIn = static_cast<float>(sample);
            hpPrevOut = hpOut;
            sample = static_cast<int16_t>(hpOut);

            // stats
            pcmData[samplesWritten++] = sample;

            if (sample < minSample) {
                minSample = sample;
            }

            if (sample > maxSample) {
                maxSample = sample;
            }

            sumSquares += static_cast<int64_t>(sample) * sample;
            measuredSamples++;
        }
    }

    // normalisation to avoid low volume audio
    int16_t peak = std::max(
        static_cast<int16_t>(std::abs(static_cast<int>(minSample))),
        static_cast<int16_t>(std::abs(static_cast<int>(maxSample)))
    );

    if (peak > 0) {
        // viser ~90% du plein-échelle pour garder une marge
        const float gain = (INT16_MAX * 0.9f) / static_cast<float>(peak);

        for (size_t i = 0; i < samplesWritten; ++i) {
            int32_t amplified = static_cast<int32_t>(pcmData[i] * gain);

            if (amplified > INT16_MAX) amplified = INT16_MAX;
            if (amplified < INT16_MIN) amplified = INT16_MIN;

            pcmData[i] = static_cast<int16_t>(amplified);
        }

        Serial.printf(
            "[AudioRecorder] Normalized, gain x%.2f\n",
            gain
        );
    }

    writeWavHeader(
        wavBuffer,
        pcmSize
    );

    Serial.printf(
        "[AudioRecorder] Recording finished: %u bytes\n",
        static_cast<unsigned>(wavSize)
    );

    double rms = 0.0;

    if (measuredSamples > 0) {
        rms = sqrt(
            static_cast<double>(sumSquares) /
            measuredSamples
        );
    }

    Serial.printf(
        "[AudioRecorder] Min: %d\n",
        minSample
    );

    Serial.printf(
        "[AudioRecorder] Max: %d\n",
        maxSample
    );

    Serial.printf(
        "[AudioRecorder] RMS: %.2f\n",
        rms
    );

    return true;
}

void AudioRecorder::writeWavHeader(
    uint8_t* buffer,
    uint32_t dataSize
)
{
    const uint32_t byteRate =
        SAMPLE_RATE *
        CHANNELS *
        BITS_PER_SAMPLE /
        8;

    const uint16_t blockAlign =
        CHANNELS *
        BITS_PER_SAMPLE /
        8;

    /*
     * RIFF
     */
    std::memcpy(
        buffer + 0,
        "RIFF",
        4
    );

    const uint32_t chunkSize =
        36 + dataSize;

    std::memcpy(
        buffer + 4,
        &chunkSize,
        4
    );

    /*
     * WAVE
     */
    std::memcpy(
        buffer + 8,
        "WAVE",
        4
    );

    /*
     * fmt
     */
    std::memcpy(
        buffer + 12,
        "fmt ",
        4
    );

    const uint32_t fmtSize = 16;

    std::memcpy(
        buffer + 16,
        &fmtSize,
        4
    );

    /*
     * PCM
     */
    const uint16_t audioFormat = 1;

    std::memcpy(
        buffer + 20,
        &audioFormat,
        2
    );

    std::memcpy(
        buffer + 22,
        &CHANNELS,
        2
    );

    std::memcpy(
        buffer + 24,
        &SAMPLE_RATE,
        4
    );

    std::memcpy(
        buffer + 28,
        &byteRate,
        4
    );

    std::memcpy(
        buffer + 32,
        &blockAlign,
        2
    );

    std::memcpy(
        buffer + 34,
        &BITS_PER_SAMPLE,
        2
    );

    /*
     * data
     */
    std::memcpy(
        buffer + 36,
        "data",
        4
    );

    std::memcpy(
        buffer + 40,
        &dataSize,
        4
    );
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
    if (wavBuffer != nullptr) {
        free(wavBuffer);
        wavBuffer = nullptr;
    }

    wavSize = 0;
}

size_t AudioRecorder::size() const
{
    return wavSize;
}

const uint8_t* AudioRecorder::data() const
{
    return wavBuffer;
}