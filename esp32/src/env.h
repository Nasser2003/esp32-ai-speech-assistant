#pragma once

struct ENV { // constexpr + const => mendatory for char* type, optional for int type

    // API
    static constexpr const char* API_HOST = "192.168.0.250";
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

    // Pins
    static constexpr const int BUTTON_PIN = 2;
    static constexpr const int BATTERY_PIN = 4;
    static constexpr const int BLUE_LED = 14;
    struct I2S_PINS {
        static constexpr const int BCLK = 18;       // shared clock (MIC SCK + SPEAKER BLCK)
        static constexpr const int WS = 17;          // shared word select (MIC MIC WS + SPEAKER LRC)
        static constexpr const int MIC_SD = 40;      // mic data in (INMP441 SD)
        static constexpr const int SPK_DOUT = 39;    // speaker data out (MAX98357A DIN)
    };
    struct SCREEN_PINS {
        static constexpr const int SDA = 15;
        static constexpr const int SCK = 7;
    };
};
