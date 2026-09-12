#include <Arduino.h>
#include <WiFi.h>
#include <WiFiProv.h>

#include "audio-player.h"
#include "audio-recorder.h"
#include "oled-screen.h"
#include "sinus-pulse.h"
#include "timer.h"
#include "state.h"
#include "env.h"
#include "websocket-controller.h"
#include "power-controller.h"

// Variables
AudioPlayer audioPlayer(ENV::SPEAKER_PINS::D_OUT_PIN, ENV::SPEAKER_PINS::BCLK_PIN, ENV::SPEAKER_PINS::LRC_PIN);
OledScreen128x32 screen(ENV::SCREEN_PINS::SDA, ENV::SCREEN_PINS::SCK, true, 150);
AudioRecorder recorder(ENV::MIC_PINS::SCK_PIN, ENV::MIC_PINS::WS_PIN, ENV::MIC_PINS::SD_PIN);
SinusPulse blueLedPulse(1, 270);
WebsocketController webSocket(ENV::API_HOST, ENV::API_PORT, 
    ENV::API_WEBSOCKET_PATH, ENV::RECORDING_START, 
    ENV::RECORDING_END);
PowerController powerController(ENV::BATTERY_PIN, 1000);

// Timers for state transitions and timeouts
Timer preInitTimer(10);
Timer initTimer(2000);
Timer connectingTimeoutTimer(1000);
Timer connectedWifiTimer(1);
Timer minRecordingTimer(2000);
Timer maxRecordingTimer(15000);
Timer recordStopTimer(1000);
Timer errorTimer(2000);
Timer updateTimer(0);
Timer batteryMeasureTimer(2000);

static bool ai_text_finished = false;
static bool ai_tts_finished = false;


// Function declarations
void setWebSocketCallback();
void setWifiCallback();
bool isButtonPressed();

void setup()
{
    changeState(State::INIT);
    Serial.begin(115200);
    delay(1000);
    
    ledcSetup(0, 5000, 8); // to use PWM feature on the led
    ledcAttachPin(ENV::BLUE_LED, 0); // PWM way to setup pinMode
    screen.init();
    preInitTimer.start();
    // WiFi.setTxPower(WIFI_POWER_8_5dBm); // Set the WiFi transmission power to 8.5 dBm to avoid the brownout effect
    audioPlayer.setVolume(15);
    setWebSocketCallback();
    setWifiCallback();
    // Set the attenuation to 11 dB for the battery pin
    analogReadResolution(12);
    analogSetPinAttenuation(ENV::BATTERY_PIN, ADC_11db); 
    pinMode(ENV::BATTERY_PIN, INPUT);
    pinMode(ENV::BUTTON_PIN, INPUT_PULLUP);
}

void loop()
{
    switch (getState())
    {
    case State::INIT:
        if (preInitTimer.isElapsed() && runOnceOnStateChange()) 
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
            changeState(State::BATTERY_MEASURE);
        }
        break;
    case State::CONNECTING_WIFI:
        if (runOnceOnStateChange()) 
        {
            screen.displayMessage("Connecting to WiFi...");
            WiFi.begin();
            connectingTimeoutTimer.start();
        }
        if (WiFi.status() == WL_CONNECTED) 
        {
            changeState(State::CONNECTED_WIFI);
        }
        if (connectingTimeoutTimer.isElapsed()) 
        {
            changeState(State::CONNECTION_FAILED);
        }
        break;
    case State::CONNECTION_FAILED:
        if (runOnceOnStateChange()) 
        {
            screen.addMessage("\n x Failed to connect to WiFi. Starting BLE provisioning...");
            errorTimer.start();
        }
        if (errorTimer.isElapsed()) 
        {
            changeState(State::BLE_PROVISIONING);
        }
        break;
    case State::CONNECTED_WIFI:
        if (runOnceOnStateChange()) 
        {
            screen.addMessage("\n v Connected to WiFi!");
            connectedWifiTimer.start();
        }
        if (connectedWifiTimer.isElapsed()) 
        {
            changeState(State::CONNECTING_API);
        }
        break;
    case State::CONNECTING_API:
        if (runOnceOnStateChange()) 
        {
            if (webSocket.connect()) {
                changeState(State::CONNECTED_API);
                webSocket.disconnect();
            } else {
                errorTimer.start();
                screen.addMessage("\n x Problem connecting to API.");
            }
        }
        else if (errorTimer.isElapsed()) 
        {
            screen.displayMessage("[SYS] Error: failed to connect to API.");
            changeState(State::ERROR);
        }
        break;
    case State::CONNECTED_API:
        if (runOnceOnStateChange()) 
        {
            screen.addMessage("\n v Connected to API!");
            connectedWifiTimer.start();
        }
        if (connectedWifiTimer.isElapsed()) 
        {
            changeState(State::BATTERY_MEASURE);
        }
        break;
    case State::BATTERY_MEASURE:
        if (getLastState() == State::CONNECTED_API) { // skip the battery measurement because we already did it during INIT
            changeState(State::IDLE);
            break;
        }

        if (runOnceOnStateChange())
        {
            batteryMeasureTimer.start();
            std::string batteryPercentage = std::to_string(powerController.getBatteryPercentage());
            screen.displayMessage("Battery: " + batteryPercentage + "%");
        }
        
        if (batteryMeasureTimer.isElapsed()) {
            if (getLastState() == State::INIT) {
                changeState(State::CONNECTING_WIFI);
            } else {
                changeState(State::IDLE);
            }
        }

        break;
    case State::IDLE:
        if (runOnceOnStateChange()) 
        {
            screen.displayMessage("Hold the button to record between 2 and 15 sec.");
            ai_text_finished = false;
            ai_tts_finished = false;
            audioPlayer.pushStream(nullptr, 0);
        }

        if (isButtonPressed()) // button pressed
        {
            changeState(State::RECORDING);
        }
        break;
    case State::RECORDING:
    {
        if (runOnceOnStateChange()) 
        {
            if (!webSocket.connect()) {
                changeState(State::CONNECTING_API);
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
            changeState(State::RECORDED);
        }
        if (maxRecordingTimer.isElapsed())
        {
            changeState(State::RECORDED);
        }
        break;
    }
    case State::RECORDED:
    {
        if (runOnceOnStateChange()) 
        {
            changeRecordedState(RecordedState::SENDING_AUDIO);
            audioPlayer.play("/button-release.wav");
            recorder.stopRecording();
            blueLedPulse.stopPulse();
            recordStopTimer.start();
        } else {
            if (getRecordedState() == RecordedState::SENDING_AUDIO) {
                // Send one recorded segment if available
                size_t chunkSize;
                const uint8_t* segment = recorder.fetchRecordedChunk(chunkSize);
        
                if (segment != nullptr) {
                    webSocket.sendAudio(segment, chunkSize);
                } else {
                    webSocket.endAudioSession();
                    changeRecordedState(RecordedState::ENDING_AUDIO);
                }
                // Blocked state untill websocket receives the end signal
            }
        }
        break;
    }
    case State::WAITING_AI_RESPONSE:
        if (runOnceOnStateChange())
        {
            screen.addMessage("\n[SYS] Waiting for AI\n");
        }
        break;
    case State::PLAY_RESPONSE:
        if (runOnceOnStateChange())
        {
            
        }
        if (!audioPlayer.isAudioPlaying() && audioPlayer.isStreamBufferEmpty())
        {
            // audioPlayer.endStream();
            changeState(State::BATTERY_MEASURE);
            webSocket.disconnect();
        }
        break;
    case State::ERROR:
        if (runOnceOnStateChange())
        {
            screen.addMessage("\nPress the button to reset.");
        }
        if (isButtonPressed()) {
            changeState(State::CONNECTING_WIFI);
        }
        break;
    case State::BLE_PROVISIONING:
        if (runOnceOnStateChange())
        {
            screen.displayMessage("[SYS] BLE provisioning mode. Use the app to send WiFi credentials.");
            WiFiProv.beginProvision(
                WIFI_PROV_SCHEME_BLE,
                WIFI_PROV_SCHEME_HANDLER_FREE_BTDM,
                WIFI_PROV_SECURITY_1,
                "abcd1234",
                "ESP32_Provisioning"
            );
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
        webSocket.update();
        powerController.update();
        // screen.displayMessage("Battery: " + std::to_string(powerController.getBatteryPercentage()) + "%"
        //     "\nadc: " + std::to_string(analogRead(BUTTON_PIN)));
        // Serial.printf("Time: %d ,ADC button: %d, MilliVolts: %d\n", millis(), analogRead(BUTTON_PIN), analogReadMilliVolts(BUTTON_PIN));
    }

    vTaskDelay(1); // Yield to other tasks
}

bool isButtonPressed() {
    return digitalRead(ENV::BUTTON_PIN) == LOW;
}

void setWebSocketCallback() {
    webSocket.setMessageCallback([](
        websockets::WebsocketsClient& client, 
        const websockets::WebsocketsMessage& message
    ) {
        bool IS_BINARY = message.isBinary();
        
        
        if (IS_BINARY) {
            Serial.print("[WebSocket] Received binary data");
            Serial.printf(" of length: %d\n", message.length());
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
        } else if (api_message == ENV::TRANSCRIPTION_START) {
            screen.displayMessage("");
        } else if (api_message == ENV::TRANSCRIPTION_END) {
        } else if (api_message == ENV::AI_TEXT_START || api_message == ENV::AI_TTS_START) {
            if (getState() != State::WAITING_AI_RESPONSE) {
                changeState(State::WAITING_AI_RESPONSE);
            }
        } else if (api_message == ENV::AI_TEXT_END || api_message == ENV::AI_TTS_END) {
            ai_text_finished = ai_text_finished || (api_message == ENV::AI_TEXT_END);
            ai_tts_finished = ai_tts_finished || (api_message == ENV::AI_TTS_END);
            if (ai_text_finished && ai_tts_finished) {
                changeState(State::PLAY_RESPONSE);
                ai_text_finished = false;
                ai_tts_finished = false;
            }
        } else {
            screen.addMessage(api_message);
        }

    });
}

void setWifiCallback() {
    WiFi.onEvent([](arduino_event_t* event) {
        switch (event->event_id) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.println("[WiFi] Connected");
            changeState(State::CONNECTED_WIFI);
            break;
        case ARDUINO_EVENT_PROV_START:
            Serial.println("[WiFiProv] Started");
            break;
        case ARDUINO_EVENT_PROV_CRED_SUCCESS:
            Serial.println("[WiFiProv] Credentials accepted");
            break;
        case ARDUINO_EVENT_PROV_CRED_FAIL:
            Serial.println("[WiFiProv] Credentials failed");
            changeState(State::ERROR);
            screen.displayMessage("[SYS] Error: BLE provisioning failed : Credentials rejected.");
            break;
            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("[WiFi] Disconnected");
            changeState(State::ERROR);
            screen.displayMessage("[SYS] Error: WiFi disconnected.");
            break;
        case ARDUINO_EVENT_PROV_END:
            Serial.println("[WiFiProv] Ended");
            break;
        }
    });
}