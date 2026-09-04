#include <Arduino.h>
#include "audio-player.h"
#include "audio-recorder.h"
#include "oled-screen.h"
#include "sinus-pulse.h"
#include "timer.h"
#include "state.h"
#include "credentials.h"
#include <WiFi.h>
#include "websocket-controller.h"

// Pins
constexpr const int BUTTON_PIN = 2;
constexpr const int BLUE_LED = 14;

// Constants
constexpr const char* RECORDING_START = "/RECORDING START";
constexpr const char* RECORDING_END = "/RECORDING END";

constexpr const char* TRANSCRIPTION_START = "/TRANSCRIPTION START";
constexpr const char* TRANSCRIPTION_END = "/TRANSCRIPTION END";

constexpr const char* AI_TEXT_START = "/AI TEXT START";
constexpr const char* AI_TEXT_END = "/AI TEXT END";

constexpr const char* AI_TTS_START = "/AI TTS START";
constexpr const char* AI_TTS_END = "/AI TTS END";

// Variables
AudioPlayer audioPlayer(39, 42, 3);
OledScreen128x32 screen(15, 7, true, 150);
AudioRecorder recorder(18,17,40);
SinusPulse blueLedPulse(1, 270);
WebsocketController webSocket(API_HOST, API_PORT, 
    API_WEBSOCKET_PATH, RECORDING_START, RECORDING_END);
bool ai_text_finished = false;
bool ai_tts_finished = false;

// Timers
Timer preInitTimer(10);
Timer initTimer(2000);
Timer connectingTimeoutTimer(3000);
Timer connected_wifi(1000);
Timer minRecordingTimer(2000);
Timer maxRecordingTimer(15000);
Timer recordStopTimer(1000);

Timer updateTimer(5);

// State machine
State state = State::INIT;
State lastState = State::PRE_INIT;
RecordedState recordedState = RecordedState::SENDING_AUDIO;


// Function declarations
static bool isButtonPressed();
static bool executeOnlyOnceOnStateChange();
void setWebSocketCallback();

void setup()
{
    state = State::INIT;
    Serial.begin(115200);
    delay(1000);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    ledcSetup(0, 5000, 8); // to use PWM feature on the led
    ledcAttachPin(BLUE_LED, 0); // PWM way to setup pinMode
    screen.init();
    preInitTimer.start();
    // WiFi.setTxPower(WIFI_POWER_8_5dBm); // Set the WiFi transmission power to 8.5 dBm to avoid the brownout effect
    audioPlayer.setVolume(10);
    setWebSocketCallback();
}

void loop()
{
    switch (state)
    {
    case State::INIT:
        if (preInitTimer.isElapsed() && executeOnlyOnceOnStateChange()) 
        {
            screen.displayMessage("Initializing the AI assistant...");
            initTimer.start();
            updateTimer.start();
            if (!audioPlayer.init()) {
                Serial.println("AudioPlayer initialization failed");
            }
            audioPlayer.startStream();
        }
        if (initTimer.isElapsed()) 
        {
            state = State::CONNECTING_WIFI;
        }
        break;
    case State::CONNECTING_WIFI:
        if (executeOnlyOnceOnStateChange()) 
        {
            screen.displayMessage("Connecting to WiFi...");
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            connectingTimeoutTimer.start();
        }
        if (WiFi.status() == WL_CONNECTED) 
        {
            state = State::CONNECTED_WIFI;
        }
        if (connectingTimeoutTimer.isElapsed()) 
        {
            state = State::CONNECTION_FAILED;
        }
        break;
    case State::CONNECTION_FAILED:
        if (executeOnlyOnceOnStateChange()) 
        {
            screen.addMessage("\n x Failed to connect to WiFi.");
        }
        break;
    case State::CONNECTED_WIFI:
        if (executeOnlyOnceOnStateChange()) 
        {
            screen.addMessage("\n v Connected to WiFi!");
            connected_wifi.start();
        }
        if (connected_wifi.isElapsed()) 
        {
            state = State::CONNECTING_API;
        }
        break;
    case State::CONNECTING_API:
        if (executeOnlyOnceOnStateChange()) 
        {
            if (webSocket.connect()) {
                state = State::CONNECTED_API;
                webSocket.disconnect();
            } else {
                screen.addMessage("\n x Problem connecting to API.");
            }
        }
        break;
    case State::CONNECTED_API:
        if (executeOnlyOnceOnStateChange()) 
        {
            screen.addMessage("\n v Connected to API!");
            connected_wifi.start();
        }
        if (connected_wifi.isElapsed()) 
        {
            state = State::IDLE;
            screen.displayMessage("Hold the button to record min 2 sec and max 15 sec.");
        }
        break;
    case State::IDLE:
        if (executeOnlyOnceOnStateChange()) 
        {
            ai_text_finished = false;
            ai_tts_finished = false;
            audioPlayer.pushStream(nullptr, 0);
        }

        if (isButtonPressed()) // button pressed
        {
            state = State::RECORDING;
        }
        break;
    case State::RECORDING:
    {
        if (executeOnlyOnceOnStateChange()) 
        {
            if (!webSocket.connect()) {
                state = State::CONNECTING_API;
            }
            audioPlayer.play("/button-press.wav");
            screen.displayMessage("Recording audio...");
            recorder.startRecording();
            blueLedPulse.startPulse();
            minRecordingTimer.start();
            maxRecordingTimer.start();

            webSocket.startAudioSession();
        } else {
            // Send one recorded segment if available
            size_t chunkSize;
            const uint8_t* segment = recorder.fetchRecordedChunk(chunkSize);
    
            if (segment != nullptr) {
                webSocket.sendAudio(segment, chunkSize);
            }
        }

        if (!isButtonPressed() && minRecordingTimer.isElapsed())
        {
            state = State::RECORDED;
        }
        if (maxRecordingTimer.isElapsed())
        {
            state = State::RECORDED;
        }
        break;
    }
    case State::RECORDED:
    {
        if (executeOnlyOnceOnStateChange()) 
        {
            recordedState = RecordedState::SENDING_AUDIO;
            audioPlayer.play("/button-release.wav");
            recorder.stopRecording();
            blueLedPulse.stopPulse();
            recordStopTimer.start();
        } else {
            if (recordedState == RecordedState::SENDING_AUDIO) {
                // Send one recorded segment if available
                size_t chunkSize;
                const uint8_t* segment = recorder.fetchRecordedChunk(chunkSize);
        
                if (segment != nullptr) {
                    webSocket.sendAudio(segment, chunkSize);
                } else {
                    webSocket.endAudioSession();
                    recordedState = RecordedState::ENDING_AUDIO;
                }
                // Blocked state untill websocket receives the end signal
            }
        }
        break;
    }
    case State::WAITING_AI_RESPONSE:
        if (executeOnlyOnceOnStateChange())
        {
            screen.displayMessage("Waiting for AI:\n");
        }
        break;
    case State::PLAY_RESPONSE:
        if (executeOnlyOnceOnStateChange())
        {
            
        }
        if (!audioPlayer.isAudioPlaying() && audioPlayer.isStreamBufferEmpty())
        {
            // audioPlayer.endStream();
            state = State::IDLE;
        }
        break;
    default:
        break;
    }

    // Update variables
    if (false || updateTimer.isElapsed()) { // I put true to skip the time for now 
        updateTimer.start();

        ledcWrite(0, blueLedPulse.getPulseState());
        recorder.update();
        screen.update();
        // audioPlayer.loop();
        webSocket.update();
    }

}

static bool executeOnlyOnceOnStateChange() {
    bool stateHasChanged = (state != lastState);
    if (stateHasChanged) {
        Serial.printf("[STATE] Change: %s -> %s\n", stateToString(lastState), stateToString(state));
        lastState = state;
        return true;
    }
    return false;
}

static bool isButtonPressed() {
    return digitalRead(BUTTON_PIN) == LOW;
}

void setWebSocketCallback() {
    webSocket.setMessageCallback([](
        websockets::WebsocketsClient& client, 
        const websockets::WebsocketsMessage& message
    ) {
        bool IS_BINARY = message.isBinary();
        
        
        if (IS_BINARY) {
            const std::string& data = message.rawData();
            audioPlayer.pushStream(
                reinterpret_cast<const uint8_t*>(data.data()),
                data.size()
            );
            return;
        }

        std::string api_message = message.data().c_str();
        if (api_message.empty()) {
            return;
        } else if (api_message == TRANSCRIPTION_START) {
            screen.displayMessage("");
        } else if (api_message == TRANSCRIPTION_END) {
        } else if (api_message == AI_TEXT_START || api_message == AI_TTS_START) {
            state = State::WAITING_AI_RESPONSE;
        } else if (api_message == AI_TEXT_END || api_message == AI_TTS_END) {
            ai_text_finished = ai_text_finished || (api_message == AI_TEXT_END);
            ai_tts_finished = ai_tts_finished || (api_message == AI_TTS_END);
            if (ai_text_finished && ai_tts_finished) {
                state = State::PLAY_RESPONSE;
                ai_text_finished = false;
                ai_tts_finished = false;
            }
        } else {
            screen.addMessage(api_message);
        }

    });
}