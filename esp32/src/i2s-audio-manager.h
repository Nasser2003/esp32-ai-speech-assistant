#pragma once

#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/semphr.h>

// Manages a single I2S port (I2S_NUM_0) shared between
// the INMP441 microphone (RX) and MAX98357A speaker (TX).
//
// Usage is strictly sequential: the manager installs/uninstalls
// the I2S driver on each RX <-> TX transition.
// BCLK and WS pins are shared; only the data pin differs.

class I2SAudioManager {
public:
    enum class Mode { NONE, RX, TX };

    I2SAudioManager(
        int bclkPin,       // shared BCLK (clock)
        int wsPin,         // shared WS (word select)
        int rxDataPin,     // mic serial data in (INMP441 SD)
        int txDataPin      // speaker serial data out (MAX98357A DIN)
    );

    // Activate RX mode (microphone).
    // If currently TX, uninstalls then reinstalls for RX.
    bool activateRX();

    // Activate TX mode (speaker).
    // If currently RX, uninstalls then reinstalls for TX.
    bool activateTX();

    // Deactivate the current mode (uninstall driver).
    void deactivate();

    Mode getCurrentMode() const;

private:
    static constexpr i2s_port_t I2S_PORT = I2S_NUM_0;

    // RX config (INMP441): 16 kHz, 32-bit slot, mono left
    static constexpr uint32_t RX_SAMPLE_RATE = 16000;
    static constexpr i2s_bits_per_sample_t RX_BITS = I2S_BITS_PER_SAMPLE_32BIT;

    // TX config (MAX98357A): 16 kHz, 16-bit, mono left
    static constexpr uint32_t TX_SAMPLE_RATE = 16000;
    static constexpr i2s_bits_per_sample_t TX_BITS = I2S_BITS_PER_SAMPLE_16BIT;

    int bclkPin;
    int wsPin;
    int rxDataPin;
    int txDataPin;

    volatile Mode currentMode;
    SemaphoreHandle_t mutex;

    bool installRX();
    bool installTX();
    void uninstallCurrent();
};
