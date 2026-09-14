#include "audio-player.h"
#include "i2s-audio-manager.h"

#include <Arduino.h>
#include <LittleFS.h>

AudioPlayer::AudioPlayer(I2SAudioManager& manager)
    : manager(manager),
      pcmQueue(nullptr),
      pcmTaskHandle(nullptr),
      streamPlaying(false),
      streamEnded(false),
      audioPlaying(false)
{
}

bool AudioPlayer::init()
{
    Serial.println("Initialisation LittleFS...");

    if (!LittleFS.begin(false)) {
        Serial.println("Error : LittleFS");
        return false;
    }

    Serial.printf(
        "Flash: %u MB\n",
        ESP.getFlashChipSize() / (1024 * 1024)
    );

    Serial.printf(
        "PSRAM: %u MB\n",
        ESP.getPsramSize() / (1024 * 1024)
    );

    // I2S is now managed by I2SAudioManager.
    // activateTX() will be called before each playback.

    
    // TTS QUEUE
    pcmQueue = xQueueCreate(
        PCM_QUEUE_LENGTH,
        sizeof(AudioChunk)
    );

    if (pcmQueue == nullptr) {
        Serial.println(
            "Error : impossible to create PCM queue"
        );
        return false;
    }

    // TTS TASK
    BaseType_t result = xTaskCreate(
        pcmTaskEntry,
        "PCM_Audio",
        4096 * 2,  // doubled: i2s_write internals need headroom
        this,
        5,
        &pcmTaskHandle
    );

    if (result != pdPASS) {
        Serial.println(
            "Error : impossible to create PCM_Audio"
        );
        return false;
    }

    Serial.println("AudioPlayer ready");

    return true;
}

bool AudioPlayer::play(const char* path)
{
    // Ensure TX mode is active before playing
    if (!manager.activateTX()) {
        Serial.println("[AudioPlayer] Failed to activate I2S TX");
        return false;
    }

    File file = LittleFS.open(path, "r");

    if (!file) {
        Serial.printf("WAV not found: %s\n", path);
        return false;
    }

    // Sauter le header WAV
    file.seek(44);

    // Static: avoids 4096-byte stack allocation; play() is always called from main task.
    // BSS alignment is ≥ 4 bytes, satisfying the int16_t reinterpret_cast below.
    static uint8_t buffer[PCM_CHUNK_SIZE];

    while (file.available()) {
        size_t bytesRead = file.read(buffer, sizeof(buffer));

        applyVolume(
            reinterpret_cast<int16_t*>(buffer),
            bytesRead / sizeof(int16_t)
        );

        size_t bytesWritten = 0;

        esp_err_t result = i2s_write(
            I2S_NUM_0,
            buffer,
            bytesRead,
            &bytesWritten,
            portMAX_DELAY
        );

        if (result != ESP_OK) {
            Serial.printf("Erreur I2S: %d\n", result);
            file.close();
            return false;
        }
    }

    file.close();

    Serial.println("Playback finished");
    return true;
}

void AudioPlayer::setVolume(uint8_t volume) {
    // volume attendu 0-100, converti en gain 0.0-1.0
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
    if (streamPlaying) {
        Serial.println("Stream déjà actif");
        return false;
    }

    // Ensure TX mode is active before streaming
    if (!manager.activateTX()) {
        Serial.println("[AudioPlayer] Failed to activate I2S TX for stream");
        return false;
    }

    // On vide les éventuels anciens chunks.
    xQueueReset(pcmQueue);

    streamEnded = false;
    streamPlaying = true;

    Serial.println("PCM stream démarré");

    return true;
}


bool AudioPlayer::pushStream(
    const uint8_t* data,
    size_t length
)
{
    if (!streamPlaying) {
        return false;
    }

    if (data == nullptr || length == 0) {
        return false;
    }

    // Static: avoids a 4100-byte stack allocation that would overflow the main
    // Arduino task stack (~8 KB) when called from the WebSocket callback chain.
    // Safe because pushStream is always called from the same task (main loop).
    static AudioChunk chunk;

    size_t offset = 0;
    while (offset < length) {
        size_t bytesToCopy = length - offset;
        if (bytesToCopy > PCM_CHUNK_SIZE) {
            bytesToCopy = PCM_CHUNK_SIZE;
        }
        memcpy(chunk.data, data + offset, bytesToCopy);
        chunk.length = bytesToCopy;
        offset += bytesToCopy;

        // Block up to 300 ms waiting for queue space; drop remaining data if full
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
    // Static sentinel: only the length==0 matters; data[] is ignored by pcmTask.
    // Static avoids another 4100-byte stack allocation in the callback chain.
    static AudioChunk sentinel;
    sentinel.length = 0;
    if (xQueueSend(pcmQueue, &sentinel, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("[AudioPlayer] endStream: failed to push sentinel (queue full)");
    } else {
        Serial.println("[AudioPlayer] endStream: sentinel queued");
    }
    streamEnded = true;
}

bool AudioPlayer::isAudioPlaying() const
{
    return audioPlaying;
}


bool AudioPlayer::isStreamPlaying() const
{
    return streamPlaying;
}


bool AudioPlayer::isStreamBufferEmpty() const
{
    if (pcmQueue == nullptr) {
        return true;
    }

    return uxQueueMessagesWaiting(pcmQueue) == 0;
}


// ======================================================
// PCM TASK
// ======================================================

void AudioPlayer::pcmTaskEntry(void* parameter)
{
    AudioPlayer* player =
        static_cast<AudioPlayer*>(parameter);

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
                // Sentinel: the current TTS batch is fully enqueued and played
                Serial.println("[PCM] Sentinel received — batch done");
                audioPlaying = false;
                streamEnded = false; // ready for next session
                continue;
            }

            audioPlaying = true;

            applyVolume(chunk->data, chunk->length / sizeof(int16_t));

            size_t bytesWritten = 0;
            esp_err_t err = i2s_write(
                I2S_NUM_0,
                chunk->data,
                chunk->length,
                &bytesWritten,
                portMAX_DELAY
            );

            if (err != ESP_OK) {
                Serial.printf("[PCM] i2s_write error: %d\n", err);
            }

            audioPlaying = false;
        }
    }
}

// EXAMPLE
// I2SAudioManager i2sManager(18, 17, 40, 39);
// AudioPlayer audioPlayer(i2sManager);

// void setup() {
//     Serial.begin(115200);
//     delay(1000);

//     if (!audioPlayer.init()) {
//         Serial.println("AudioPlayer initialization failed");

//         while (true) {
//             delay(1000);
//         }
//     }
    
//     audioPlayer.setVolume(10);

//     audioPlayer.play("/the_perfect_christmas.wav");
// }

// void loop() {
//     vTaskDelay(1);
// }