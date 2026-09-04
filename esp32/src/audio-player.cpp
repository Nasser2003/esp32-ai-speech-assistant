#include "audio-player.h"

#include <LittleFS.h>
#include "Audio.h"

AudioPlayer::AudioPlayer(
    int doutPin,
    int bclkPin,
    int lrcPin
)
    : doutPin(doutPin),
      bclkPin(bclkPin),
      lrcPin(lrcPin),
      pcmQueue(nullptr),
      pcmTaskHandle(nullptr),
      streamPlaying(false),
      streamEnded(false)
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

    // I2S pour PCM/TTS
    if (!configureI2S()) {
        Serial.println("Error : configuration I2S");
        return false;
    }

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
    BaseType_t result = xTaskCreatePinnedToCore(
        pcmTaskEntry,
        "PCM_Audio",
        8192,
        this,
        5,
        &pcmTaskHandle,
        1
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
    File file = LittleFS.open(path, "r");

    if (!file) {
        Serial.printf("WAV not found: %s\n", path);
        return false;
    }

    // Sauter le header WAV
    file.seek(44);

    uint8_t buffer[PCM_CHUNK_SIZE];

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

    Serial.println("Lecture terminée");
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
        Serial.println("Erreur : stream PCM non démarré");
        return false;
    }

    if (data == nullptr || length == 0) {
        return false;
    }

    size_t offset = 0;

    while (offset < length) {

        size_t chunkLength =
            min(
                length - offset,
                PCM_CHUNK_SIZE
            );

        AudioChunk chunk;

        chunk.length = chunkLength;

        memcpy(
            chunk.data,
            data + offset,
            chunkLength
        );

        // Attendre si la queue est pleine.
        //
        // Cela crée naturellement un back-pressure :
        // si l'ESP32 lit moins vite que le serveur n'envoie,
        // le thread WebSocket finira par attendre.
        if (xQueueSend(
                pcmQueue,
                &chunk,
                portMAX_DELAY
            ) != pdTRUE)
        {
            Serial.println("Error sending to PCM queue");
            return false;
        }

        offset += chunkLength;
    }

    return true;
}


void AudioPlayer::endStream()
{
    streamEnded = true;

    Serial.println("End of PCM stream received");
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
    AudioChunk chunk;

    while (true) {

        // Attend jusqu'à recevoir un chunk.
        if (xQueueReceive(
                pcmQueue,
                &chunk,
                portMAX_DELAY
            ) == pdTRUE)
        {
            audioPlaying = true;

            applyVolume(
                reinterpret_cast<int16_t*>(chunk.data),
                chunk.length / sizeof(int16_t)
            );

            size_t offset = 0;

            while (offset < chunk.length) {

                size_t bytesWritten = 0;

                esp_err_t result = i2s_write(
                    I2S_NUM_0,
                    chunk.data + offset,
                    chunk.length - offset,
                    &bytesWritten,
                    portMAX_DELAY
                );

                if (result != ESP_OK) {
                    Serial.printf(
                        "Erreur I2S : %d\n",
                        result
                    );

                    break;
                }

                offset += bytesWritten;
            }

            audioPlaying = false;
        }


        if (
            streamEnded &&
            uxQueueMessagesWaiting(pcmQueue) == 0
        ) {
            streamPlaying = false;
            streamEnded = false;

            Serial.println("PCM stream terminé");
        }
    }
}

// ======================================================
// I2S
// ======================================================

bool AudioPlayer::configureI2S()
{
    i2s_config_t config = {
        .mode =
            (i2s_mode_t)(
                I2S_MODE_MASTER |
                I2S_MODE_TX
            ),

        .sample_rate = 16000,

        .bits_per_sample =
            I2S_BITS_PER_SAMPLE_16BIT,

        .channel_format =
            I2S_CHANNEL_FMT_ONLY_LEFT,

        .communication_format =
            I2S_COMM_FORMAT_STAND_I2S,

        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,

        .dma_buf_count = 16,

        .dma_buf_len = 256,

        .use_apll = false,

        .tx_desc_auto_clear = true,

        .fixed_mclk = 0
    };

    esp_err_t result = i2s_driver_install(
        I2S_NUM_0,
        &config,
        0,
        nullptr
    );

    if (result != ESP_OK) {
        Serial.printf(
            "Erreur i2s_driver_install : %d\n",
            result
        );
        return false;
    }

    i2s_pin_config_t pins = {
        .bck_io_num = bclkPin,
        .ws_io_num = lrcPin,
        .data_out_num = doutPin,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    result = i2s_set_pin(
        I2S_NUM_0,
        &pins
    );

    if (result != ESP_OK) {
        Serial.printf(
            "Erreur i2s_set_pin : %d\n",
            result
        );

        return false;
    }

    return true;
}

// EXAMPLE
// constexpr int I2S_DOUT = 39;
// constexpr int I2S_BCLK = 42;
// constexpr int I2S_LRC = 3;

// AudioPlayer audioPlayer(
//     I2S_DOUT,
//     I2S_BCLK,
//     I2S_LRC
// );

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
//     audioPlayer.loop();
//     vTaskDelay(1);
// }