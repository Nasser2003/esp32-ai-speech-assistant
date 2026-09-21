#include "audio-recorder.h"
#include "i2s-audio-manager.h"

#include <Arduino.h>
#include <cmath>
#include <climits>

constexpr uint32_t AudioRecorder::SAMPLE_RATE;
constexpr uint16_t AudioRecorder::BITS_PER_SAMPLE;
constexpr uint16_t AudioRecorder::CHANNELS;
constexpr uint32_t AudioRecorder::DEFAULT_TIMEOUT;

AudioRecorder::AudioRecorder(I2SAudioManager& manager)
    : manager(manager),
      recordingStartTime(0),
      recordingTimeout(0),
      hpPrevIn(0.0f),
      hpPrevOut(0.0f),
      writePos(0),
      activeBuf(0),
      readyBuf(-1),
      readySize(0),
      samplesWritten(0),
      initialized(false),
      isRecording(false)
{
    init();
}

bool AudioRecorder::init()
{
    initialized = true;
    Serial.println("[AudioRecorder] Initialized (I2S managed by I2SAudioManager)");
    return true;
}

bool AudioRecorder::startRecording(uint32_t timeoutSeconds)
{
    if (!initialized) {
        Serial.println("[AudioRecorder] Not initialized");
        return false;
    }

    if (isRecording) {
        Serial.println("[AudioRecorder] Already recording");
        return false;
    }

    clear();

    if (!manager.activateRX()) {
        Serial.println("[AudioRecorder] Failed to activate I2S RX");
        return false;
    }

    i2s_zero_dma_buffer(I2S_PORT);

    samplesWritten = 0;

    hpPrevIn = 0.0f;
    hpPrevOut = 0.0f;

    recordingStartTime = millis();
    recordingTimeout = timeoutSeconds * 1000UL;
    isRecording = true;

    Serial.printf("[AudioRecorder] Recording started (timeout: %lu s)\n", timeoutSeconds);
    return true;
}

void AudioRecorder::update()
{
    if (!isRecording) {
        return;
    }

    if (millis() - recordingStartTime >= recordingTimeout) {
        Serial.println("[AudioRecorder] Recording timeout");
        stopRecording();
        return;
    }

    int32_t rawSamples[RAW_SAMPLE_COUNT];
    size_t bytesRead = 0;

    const esp_err_t result = i2s_read(I2S_PORT, rawSamples, sizeof(rawSamples), &bytesRead, 0);

    if (result != ESP_OK) {
        Serial.printf("[AudioRecorder] i2s_read failed: %d\n", result);
        stopRecording();
        return;
    }

    if (bytesRead == 0) {
        return;
    }

    const size_t samplesRead = bytesRead / sizeof(int32_t);

    for (size_t i = 0; i < samplesRead; ++i) {

        // INMP441 : 24 bits utiles dans un slot de 32 bits -> 16 bits PCM
        int16_t sample = static_cast<int16_t>(rawSamples[i] >> 16);

        // High-pass filter
        const float hpOut = HP_ALPHA * (hpPrevOut + sample - hpPrevIn);
        hpPrevIn = static_cast<float>(sample);
        hpPrevOut = hpOut;
        sample = static_cast<int16_t>(hpOut);

        // Preamp gain
        int32_t boosted = static_cast<int32_t>(sample) * PREAMP_GAIN;
        if (boosted > INT16_MAX) boosted = INT16_MAX;
        if (boosted < INT16_MIN) boosted = INT16_MIN;
        sample = static_cast<int16_t>(boosted);

        // Écriture dans le buffer actif
        chunkBuf[activeBuf][writePos++] = sample;
        samplesWritten++;

        if (writePos >= RECORDED_SAMPLE_CHUNK_SIZE) {
            if (readyBuf != -1) {
                // Le chunk précédent n'a pas encore été récupéré : on le
                // sacrifie plutôt que de bloquer la capture temps réel.
                Serial.println("[AudioRecorder] Chunk overwritten (fetch trop lent)");
            }

            readyBuf = activeBuf;
            readySize = writePos;

            activeBuf = 1 - activeBuf;
            writePos = 0;
        }
    }
}

const uint8_t* AudioRecorder::fetchRecordedChunk(size_t& size)
{
    // Un chunk plein est prêt.
    if (readyBuf != -1) {
        const int buf = readyBuf;
        size = readySize * sizeof(int16_t);
        readyBuf = -1;
        return reinterpret_cast<const uint8_t*>(chunkBuf[buf]);
    }

    // Enregistrement terminé : flush du reliquat, une seule fois.
    if (!isRecording && writePos > 0) {
        size = writePos * sizeof(int16_t);
        writePos = 0;
        return reinterpret_cast<const uint8_t*>(chunkBuf[activeBuf]);
    }

    size = 0;
    return nullptr;
}

bool AudioRecorder::stopRecording()
{
    if (!isRecording) {
        return false;
    }

    isRecording = false;

    // Switch I2S back to TX (speaker) immediately so play() can start without delay
    manager.activateTX();

    Serial.printf(
        "[AudioRecorder] Recording finished: %u samples (~%.2fs)\n",
        static_cast<unsigned>(samplesWritten),
        static_cast<float>(samplesWritten) / SAMPLE_RATE
    );

    return true;
}

void AudioRecorder::clear()
{
    if (isRecording) {
        Serial.println("[AudioRecorder] Cannot clear while recording");
        return;
    }

    writePos = 0;
    activeBuf = 0;
    readyBuf = -1;
    readySize = 0;
    samplesWritten = 0;
}

size_t AudioRecorder::getSize() const
{
    return samplesWritten * sizeof(int16_t);
}

bool AudioRecorder::isRecordingState() const
{
    return isRecording;
}