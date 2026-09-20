#include "audio-player.h"
#include "i2s-audio-manager.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <cstring>

AudioPlayer::AudioPlayer(I2SAudioManager& manager)
    : manager(manager),
      pcmQueue(nullptr),
      pcmTaskHandle(nullptr),
      filePlaybackTaskHandle(nullptr),
      streamPlaying(false),
      streamEnded(false),
      audioPlaying(false),
      wavPlaying(false),
      stopRequested(false),
      ttsBusy(false)
{
    filePath[0] = '\0';
}

bool AudioPlayer::init()
{
    Serial.println("Initialisation LittleFS...");

    if (!LittleFS.begin(false)) {
        Serial.println("Error : LittleFS");
        return false;
    }

    Serial.printf("Flash: %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("PSRAM: %u MB\n", ESP.getPsramSize() / (1024 * 1024));

    pcmQueue = xQueueCreate(PCM_QUEUE_LENGTH, sizeof(AudioChunk));

    if (pcmQueue == nullptr) {
        Serial.println("Error : impossible to create PCM queue");
        return false;
    }

    BaseType_t result = xTaskCreate(
        pcmTaskEntry, "PCM_Audio", 4096 * 2, this, 5, &pcmTaskHandle
    );

    if (result != pdPASS) {
        Serial.println("Error : impossible to create PCM_Audio");
        return false;
    }

    Serial.println("AudioPlayer ready");
    return true;
}

// ======================================================
// FILE PLAYBACK — streamé via la même queue/tâche que le TTS
// ======================================================

bool AudioPlayer::play(const char* path)
{
    if (path == nullptr || wavPlaying || ttsBusy) {
        return false;
    }

    if (!LittleFS.exists(path)) {
        Serial.printf("WAV not found: %s\n", path);
        return false;
    }

    if (!manager.activateTX()) {
        Serial.println("[AudioPlayer] Failed to activate I2S TX");
        return false;
    }

    strncpy(filePath, path, MAX_PATH_LEN - 1);
    filePath[MAX_PATH_LEN - 1] = '\0';

    stopRequested = false;
    wavPlaying = true;

    xQueueReset(pcmQueue);

    BaseType_t result = xTaskCreate(
        filePlaybackTaskEntry, "WAV_File", 4096, this, 4, &filePlaybackTaskHandle
    );

    if (result != pdPASS) {
        Serial.println("[AudioPlayer] Failed to create WAV_File task");
        wavPlaying = false;
        return false;
    }

    return true; // retourne immédiatement, la lecture se fait en tâche de fond
}

void AudioPlayer::filePlaybackTaskEntry(void* parameter)
{
    AudioPlayer* player = static_cast<AudioPlayer*>(parameter);
    player->filePlaybackTask();
    vTaskDelete(nullptr);
}

void AudioPlayer::filePlaybackTask()
{
    File file = LittleFS.open(filePath, "r");

    if (!file) {
        Serial.printf("[AudioPlayer] Cannot open %s\n", filePath);
        wavPlaying = false;
        return;
    }

    file.seek(44); // skip WAV header

    AudioChunk chunk;

    while (file.available() && !stopRequested) {
        chunk.length = file.read(
            reinterpret_cast<uint8_t*>(chunk.data),
            PCM_CHUNK_SIZE
        );

        if (chunk.length == 0) {
            break;
        }

        // Attente bornée + re-check stopRequested : évite de bloquer
        // indéfiniment si stop() reset la queue pendant l'attente.
        while (xQueueSend(pcmQueue, &chunk, pdMS_TO_TICKS(200)) != pdTRUE) {
            if (stopRequested) {
                break;
            }
        }

        if (stopRequested) {
            break;
        }
    }

    file.close();

    // Sentinelle : signale la fin du fichier à pcmTask()
    chunk.length = 0;
    xQueueSend(pcmQueue, &chunk, pdMS_TO_TICKS(500));

    Serial.println(stopRequested ? "Playback stopped" : "Playback finished");
    wavPlaying = false;
}

void AudioPlayer::stop()
{
    stopRequested = true;
    i2s_zero_dma_buffer(I2S_NUM_0);

    if (pcmQueue != nullptr) {
        xQueueReset(pcmQueue);
    }

    audioPlaying = false;
    streamPlaying = false;
    // wavPlaying repasse à false tout seul quand filePlaybackTask() se termine.
}

void AudioPlayer::setVolume(uint8_t volume) {
    volumeGain = static_cast<float>(volume) / 100.0f;
}

void AudioPlayer::applyVolume(int16_t* samples, size_t sampleCount) {
    for (size_t i = 0; i < sampleCount; ++i) {
        int32_t scaled = static_cast<int32_t>(samples[i] * volumeGain);
        if (scaled > INT16_MAX) scaled = INT16_MAX;
        if (scaled < INT16_MIN) scaled = INT16_MIN;
        samples[i] = static_cast<int16_t>(scaled);
    }
}

// ======================================================
// TTS STREAM
// ======================================================

bool AudioPlayer::startStream()
{
    if (wavPlaying) {
        Serial.println("Stream déjà actif (ou fichier en cours)");
        return false;
    }

    if (!manager.activateTX()) {
        Serial.println("[AudioPlayer] Failed to activate I2S TX for stream");
        return false;
    }

    xQueueReset(pcmQueue);

    streamEnded = false;
    stopRequested = false;
    streamPlaying = true;

    Serial.println("PCM stream démarré");
    return true;
}

bool AudioPlayer::pushStream(const uint8_t* data, size_t length)
{
    if (!streamPlaying || wavPlaying) {
        return false;
    }

    ttsBusy = true;

    if (data == nullptr || length == 0) {
        return false;
    }

    static AudioChunk chunk;

    size_t offset = 0;
    while (offset < length) {
        if (stopRequested) {
            return false;
        }

        size_t bytesToCopy = length - offset;
        if (bytesToCopy > PCM_CHUNK_SIZE) {
            bytesToCopy = PCM_CHUNK_SIZE;
        }
        memcpy(chunk.data, data + offset, bytesToCopy);
        chunk.length = bytesToCopy;
        offset += bytesToCopy;

        if (xQueueSend(pcmQueue, &chunk, pdMS_TO_TICKS(300)) != pdTRUE) {
            Serial.printf(
                "[AudioPlayer] Queue full, dropped %u remaining bytes\n",
                static_cast<unsigned>(length - offset + bytesToCopy)
            );
            return false;
        }
    }
    return true;
}

void AudioPlayer::endStream()
{
    static AudioChunk sentinel;
    sentinel.length = 0;
    if (xQueueSend(pcmQueue, &sentinel, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("[AudioPlayer] endStream: failed to push sentinel (queue full)");
    } else {
        Serial.println("[AudioPlayer] endStream: sentinel queued");
    }
    streamEnded = true;
}

bool AudioPlayer::isPlaying() const { return wavPlaying || ttsBusy || audioPlaying; }
bool AudioPlayer::isAudioPlaying() const { return isPlaying(); }
bool AudioPlayer::isStreamPlaying() const { return streamPlaying; }

bool AudioPlayer::isStreamBufferEmpty() const
{
    if (pcmQueue == nullptr) return true;
    return uxQueueMessagesWaiting(pcmQueue) == 0;
}

// ======================================================
// PCM TASK — consommateur unique : fichiers ET stream TTS
// ======================================================

void AudioPlayer::pcmTaskEntry(void* parameter)
{
    AudioPlayer* player = static_cast<AudioPlayer*>(parameter);
    player->pcmTask();
    vTaskDelete(nullptr);
}

void AudioPlayer::pcmTask()
{
    Serial.println("[PCM] Task started");

    AudioChunk* chunk = static_cast<AudioChunk*>(malloc(sizeof(AudioChunk)));

    if (chunk == nullptr) {
        Serial.println("[PCM] malloc FAILED — task aborted");
        vTaskDelete(nullptr);
        return;
    }

    while (true) {
        if (xQueueReceive(pcmQueue, chunk, portMAX_DELAY) == pdTRUE) {

            if (chunk->length == 0) {
                Serial.println("[PCM] Sentinel received — batch done");
                i2s_zero_dma_buffer(I2S_NUM_0);
                audioPlaying = false;
                streamEnded = false;
                ttsBusy = false;
                continue;
            }

            audioPlaying = true;
            applyVolume(chunk->data, chunk->length / sizeof(int16_t));

            size_t bytesWritten = 0;
            esp_err_t err = i2s_write(
                I2S_NUM_0, chunk->data, chunk->length, &bytesWritten, portMAX_DELAY
            );

            if (err != ESP_OK) {
                Serial.printf("[PCM] i2s_write error: %d\n", err);
            }

            audioPlaying = false;
        }
    }
}