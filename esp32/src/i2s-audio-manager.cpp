#include "i2s-audio-manager.h"

I2SAudioManager::I2SAudioManager(
    int bclkPin,
    int wsPin,
    int rxDataPin,
    int txDataPin
)
    : bclkPin(bclkPin),
      wsPin(wsPin),
      rxDataPin(rxDataPin),
      txDataPin(txDataPin),
      currentMode(Mode::NONE)
{
    mutex = xSemaphoreCreateMutex();
}

bool I2SAudioManager::activateRX()
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    if (currentMode == Mode::RX) {
        xSemaphoreGive(mutex);
        return true;
    }

    uninstallCurrent();

    bool ok = installRX();

    if (ok) {
        currentMode = Mode::RX;
        Serial.println("[I2SManager] Activated RX (mic)");
    } else {
        Serial.println("[I2SManager] Failed to activate RX");
    }

    xSemaphoreGive(mutex);
    return ok;
}

bool I2SAudioManager::activateTX()
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    if (currentMode == Mode::TX) {
        xSemaphoreGive(mutex);
        return true;
    }

    uninstallCurrent();

    bool ok = installTX();

    if (ok) {
        currentMode = Mode::TX;
        Serial.println("[I2SManager] Activated TX (speaker)");
    } else {
        Serial.println("[I2SManager] Failed to activate TX");
    }

    xSemaphoreGive(mutex);
    return ok;
}

void I2SAudioManager::deactivate()
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    uninstallCurrent();
    Serial.println("[I2SManager] Deactivated");

    xSemaphoreGive(mutex);
}

I2SAudioManager::Mode I2SAudioManager::getCurrentMode() const
{
    return currentMode;
}

// ============================================================
// Private
// ============================================================

bool I2SAudioManager::installRX()
{
    // INMP441 config: master + RX, 16 kHz, 32-bit slot, mono left
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
    config.sample_rate = RX_SAMPLE_RATE;
    config.bits_per_sample = RX_BITS;
    config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = 256;
    config.use_apll = false;
    config.tx_desc_auto_clear = false;
    config.fixed_mclk = 0;

    esp_err_t result = i2s_driver_install(I2S_PORT, &config, 0, nullptr);
    if (result != ESP_OK) {
        Serial.printf("[I2SManager] i2s_driver_install RX failed: %d\n", result);
        return false;
    }

    i2s_pin_config_t pins = {};
    pins.bck_io_num = bclkPin;
    pins.ws_io_num = wsPin;
    pins.data_in_num = rxDataPin;
    pins.data_out_num = I2S_PIN_NO_CHANGE;

    result = i2s_set_pin(I2S_PORT, &pins);
    if (result != ESP_OK) {
        Serial.printf("[I2SManager] i2s_set_pin RX failed: %d\n", result);
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    i2s_zero_dma_buffer(I2S_PORT);

    return true;
}

bool I2SAudioManager::installTX()
{
    // MAX98357A config: master + TX, 16 kHz, 16-bit, mono left
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = TX_SAMPLE_RATE;
    config.bits_per_sample = TX_BITS;
    config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 16;
    config.dma_buf_len = 256;
    config.use_apll = false;
    config.tx_desc_auto_clear = true;
    config.fixed_mclk = 0;

    esp_err_t result = i2s_driver_install(I2S_PORT, &config, 0, nullptr);
    if (result != ESP_OK) {
        Serial.printf("[I2SManager] i2s_driver_install TX failed: %d\n", result);
        return false;
    }

    i2s_pin_config_t pins = {};
    pins.bck_io_num = bclkPin;
    pins.ws_io_num = wsPin;
    pins.data_out_num = txDataPin;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    result = i2s_set_pin(I2S_PORT, &pins);
    if (result != ESP_OK) {
        Serial.printf("[I2SManager] i2s_set_pin TX failed: %d\n", result);
        i2s_driver_uninstall(I2S_PORT);
        return false;
    }

    return true;
}

void I2SAudioManager::uninstallCurrent()
{
    if (currentMode != Mode::NONE) {
        i2s_driver_uninstall(I2S_PORT);
        currentMode = Mode::NONE;
    }
}
