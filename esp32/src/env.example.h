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

    // Pins
    static constexpr const int BUTTON_PIN = 2;
    static constexpr const int BATTERY_PIN = 4;
    static constexpr const int BLUE_LED = 14;
    struct SPEAKER_PINS {
        static constexpr const int D_OUT_PIN = 39;
        static constexpr const int BCLK_PIN = 42;
        static constexpr const int LRC_PIN = 3;
    };
    struct SCREEN_PINS {
        static constexpr const int SDA = 15;
        static constexpr const int SCK = 7;
    };
    struct MIC_PINS {
        static constexpr const int SCK_PIN = 18;
        static constexpr const int WS_PIN = 17;
        static constexpr const int SD_PIN = 40;
    };
};
