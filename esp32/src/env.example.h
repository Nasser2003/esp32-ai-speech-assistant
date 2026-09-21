#pragma once

struct ENV { // constexpr + const => mendatory for char* type, optional for int type

    // API
    static constexpr const char* API_HOST = "192.168.0.1";
    static constexpr const int API_PORT = 5000;
    static constexpr const char* API_WEBSOCKET_PATH = "/ws/esp32";

    // WebSocket signals
    static constexpr const char* RECORDING_START = "/RECORDING START";
    static constexpr const char* RECORDING_END = "/RECORDING END";
    static constexpr const char* TRANSCRIPTION_START = "/TRANSCRIPTION START";
    static constexpr const char* TRANSCRIPTION_END = "/TRANSCRIPTION END";
    static constexpr const char* AI_TEXT_START = "/AI TEXT START";
    static constexpr const char* AI_TEXT_END = "/AI TEXT END";
    static constexpr const char* AI_TTS_START = "/AI TTS START";
    static constexpr const char* AI_TTS_END = "/AI TTS END";
    static constexpr const char* AI_WAKE_UP = "/AI WAKE UP";
    static constexpr const char* ARGUMENT = "/ARGUMENT";

    // constants
    static constexpr const int WAKEUP_INTERVAL = 10000; // seconds
    static constexpr const int SLEEP_TIMEOUT = 30000; // seconds
    static constexpr const uint32_t LOCATION_TIME_EXPIRATION_MS = 10 * 60 * 1000; // 10 minutes (ms)

    // Pins
    static constexpr const int BUTTON_PIN = 5;
    static constexpr const int BATTERY_PIN = 4;
    static constexpr const int BLUE_LED = 8;
    struct I2S_PINS {
        static constexpr const int BCLK = 1;       // shared clock (MIC SCK + SPEAKER BLCK)
        static constexpr const int WS = 0;          // shared word select (MIC MIC WS + SPEAKER LRC)
        static constexpr const int MIC_SD = 10;      // mic data in (INMP441 SD)
        static constexpr const int SPK_DOUT = 3;    // speaker data out (MAX98357A DIN)
    };
    struct SCREEN_PINS {
        static constexpr const int SDA = 6;
        static constexpr const int SCK = 7;
    };
};
