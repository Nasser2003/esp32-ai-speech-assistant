#include <Arduino.h>
#include <WiFi.h>
#include <WiFiProv.h>

#include "i2s-audio-manager.h"
#include "audio-player.h"
#include "audio-recorder.h"
#include "oled-screen.h"
#include "sinus-pulse.h"
#include "timer.h"
#include "state.h"
#include "env.h"
#include "websocket-controller.h"
#include "http-controller.h"
#include "power-controller.h"

// Controllers
I2SAudioManager i2sManager(
    ENV::I2S_PINS::BCLK,
    ENV::I2S_PINS::WS,
    ENV::I2S_PINS::MIC_SD,
    ENV::I2S_PINS::SPK_DOUT
);
AudioPlayer audioPlayer(i2sManager);
OledScreen128x32 screen(ENV::SCREEN_PINS::SDA, ENV::SCREEN_PINS::SCK, true, 150);
AudioRecorder recorder(i2sManager);
SinusPulse blueLedPulse(1, 270);
WebsocketController webSocket(ENV::API_HOST, ENV::API_PORT, 
    ENV::API_WEBSOCKET_PATH, ENV::RECORDING_START, 
    ENV::RECORDING_END);
HttpController httpController(ENV::API_HOST, ENV::API_PORT);
PowerController powerController(ENV::BATTERY_PIN, ENV::WAKEUP_INTERVAL, ENV::BUTTON_PIN, 1000);

// Timers for state transitions and timeouts
Timer preInitTimer(10);
Timer initTimer(2000);
Timer initbatteryTimer(500);
Timer connectingTimer(500);
Timer connectingTimeout(2000);
Timer connectedWifiTimer(500);
Timer minRecordingTimer(2000);
Timer maxRecordingTimer(15000);
Timer recordStopTimer(1000);
Timer afterAiResponseTimer(1000);
Timer errorTimer(2000);
Timer enterSleepModeTimer(ENV::SLEEP_TIMEOUT);
Timer sleepTimeout(2000);
Timer fetchTimer(2000);
Timer waitingAiTimeout(30000);
Timer alarmTimeout(30000);
Timer alarmOverDelay(1000);

Timer updateTimer(0);

// Variables
static bool ai_text_finished = false;
static bool ai_tts_finished = false;
static bool isSleeping = false;
static EspWakeUpCause WAKE_UP_CAUSE;
// Varialbe stored in RTC memory (kept even after deep sleep). 
// If API connection was successful, allow periodic timer wake up for api update
RTC_DATA_ATTR bool isAPIConnectionSuccessful = false;

// Function declarations
void setWebSocketCallback();
void setWifiCallback();
bool isButtonPressed();
void initComponents();

void setup()
{
    Serial.begin(115200);

    // WiFi.setTxPower(WIFI_POWER_8_5dBm); // Set the WiFi transmission power to 8.5 dBm
    setWebSocketCallback();
    setWifiCallback();
    
    analogReadResolution(12); 
    analogSetPinAttenuation(ENV::BATTERY_PIN, ADC_11db); // Set the attenuation to 11 dB for the battery pin
    pinMode(ENV::BATTERY_PIN, INPUT);
    pinMode(ENV::BUTTON_PIN, INPUT_PULLUP);
    ledcSetup(0, 5000, 8); // to use PWM feature on the led
    ledcAttachPin(ENV::BLUE_LED, 0); // PWM way to setup pinMode
}

void loop()
{
    switch (getState())
    {
    case State::NONE:
        if (runOnceOnStateChange()) 
        {
            WAKE_UP_CAUSE = powerController.getWakeUpCause();
            if (WAKE_UP_CAUSE == EspWakeUpCause::GPIO) 
            {
                Serial.println("[WAKEUP] Wakeup cause: GPIO");
                // Speed up some timers to init esp faster after deep sleep
                preInitTimer.setDuration(0);
                initTimer.setDuration(0);
                initbatteryTimer.setDuration(0);
                preInitTimer.start();
            } 
            else if (WAKE_UP_CAUSE == EspWakeUpCause::TIMER) 
            {
                Serial.println("[WAKEUP] Wakeup cause: TIMER");
                preInitTimer.setDuration(0);
                initTimer.setDuration(0);
                initbatteryTimer.setDuration(0);
                connectingTimer.setDuration(0);
                connectedWifiTimer.setDuration(0);
                enterSleepModeTimer.setDuration(0);
                changeState(State::CONNECTING_WIFI);
                break;
            } 
            else 
            {
                Serial.println("[WAKEUP] Wakeup cause: RESET");
                preInitTimer.start();
            }
        }
        if (preInitTimer.isElapsed()) 
        {
            changeState(State::INIT);
        }
    case State::INIT:
        if (runOnceOnStateChange()) 
        {
            updateTimer.start();
            initComponents();

            if (WAKE_UP_CAUSE == EspWakeUpCause::RESET) 
            {
                screen.displayMessage("Initializing...");
            }
            
            initbatteryTimer.start();
            initTimer.start();
        }
        if (initbatteryTimer.isElapsed()) 
        {
            initbatteryTimer.breakIt();
            std::string batteryPercentage = std::to_string(powerController.getBatteryPercentage());
            if (WAKE_UP_CAUSE == EspWakeUpCause::RESET) 
            {
                screen.addMessage("\nBattery: " + batteryPercentage + "%");
            }
        }
        if (initTimer.isElapsed()) 
        {
            changeState(State::CONNECTING_WIFI);
        }
        break;
    case State::CONNECTING_WIFI:
        if (runOnceOnStateChange()) 
        {
            Serial.print("Wakeup cause: ");
            Serial.println((int)powerController.getWakeUpCause());
            screen.displayMessage("Connecting to WiFi...");
            WiFi.begin();
            connectingTimeout.start();
            connectingTimer.start();
        }
        if (WiFi.status() == WL_CONNECTED && connectingTimer.isElapsed()) 
        {
            changeState(State::CONNECTED_WIFI);
        }
        if (connectingTimeout.isElapsed()) 
        {
            changeState(State::CONNECTION_FAILED);
        }
        break;
    case State::CONNECTION_FAILED:
        if (runOnceOnStateChange()) 
        {
            screen.addMessage("\n x Failed to connect to WiFi. Starting BLE provisioning...", true);
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
            screen.addMessage("\n v Connected to WiFi!", true);
            connectedWifiTimer.start();
        }
        if (WAKE_UP_CAUSE == EspWakeUpCause::TIMER) 
        {
            changeState(State::FETCH_API_UPDATES);
            break;
        }
        if (connectedWifiTimer.isElapsed()) 
        {
            changeState(State::CONNECTING_API);
            break;
        }
        break;
    case State::CONNECTING_API:
        if (runOnceOnStateChange()) 
        {
            errorTimer.start();
        }
        if (webSocket.connect()) {
            changeState(State::CONNECTED_API);
            webSocket.disconnect();
            if (WAKE_UP_CAUSE != EspWakeUpCause::TIMER) {
                isAPIConnectionSuccessful = true;
            }
        }
        if (errorTimer.isElapsed()) {
            screen.addMessage("\n x Problem connecting to API.", true);
            changeState(State::ERROR);
            if (WAKE_UP_CAUSE != EspWakeUpCause::TIMER) {
                isAPIConnectionSuccessful = false;
            }
        }
        break;
    case State::CONNECTED_API:
        if (runOnceOnStateChange()) 
        {
            screen.addMessage("\n v Connected to API!", true);
            connectedWifiTimer.start();
        }
        if (connectedWifiTimer.isElapsed()) 
        {
            changeState(State::FETCH_API_UPDATES);
            break;
        }
        break;
    case State::FETCH_API_UPDATES:
        if (runOnceOnStateChange())
        {
            fetchTimer.start();
            Serial.println("[STATE] Fetching API updates...");
            Task task;
            const int statusCode = httpController.getCurrentTask(task);
            if (statusCode >= 200 && statusCode < 300) {
                Serial.printf("[STATE] Current task: %s (%s)\n",
                    task.taskType.c_str(),
                    task.hasArgument ? task.argument.c_str() : "null");
            } else {
                Serial.printf("[STATE] Current task request failed: %d\n", statusCode);
            }
            if (task.taskType == "ALARM") {
                changeState(State::ALARM_MODE);
                if (WAKE_UP_CAUSE == EspWakeUpCause::TIMER) {
                    initComponents();
                    changeState(State::ALARM_MODE);
                }
            }
        }
        if (fetchTimer.isElapsed()) {
            if (WAKE_UP_CAUSE == EspWakeUpCause::TIMER) {
                changeState(State::SLEEP_MODE);
            } else {
                changeState(State::IDLE);
            }
        }
        break;
    case State::IDLE:
        if (runOnceOnStateChange())
        {
            std::string batteryPercentage = std::to_string(powerController.getBatteryPercentage());
            screen.displayMessage(
                "Hold the button to record 2 - 15 seconds.\n\n"
                "Battery: " + batteryPercentage + "%"
            );
            ai_text_finished = false;
            ai_tts_finished = false;
            audioPlayer.pushStream(nullptr, 0);
            WiFi.setSleep(true);
            enterSleepModeTimer.start();
        }
        if (isButtonPressed()) // button pressed
        {
            changeState(State::RECORDING);
        }
        if (enterSleepModeTimer.isElapsed()) 
        {
            changeState(State::SLEEP_MODE);
        }
        break;
    case State::RECORDING:
    {
        if (runOnceOnStateChange()) 
        {
            WiFi.setSleep(false);
            if (!WiFi.isConnected()) {
                changeState(State::CONNECTING_WIFI);
                break;
            }
            if (!webSocket.connect()) {
                changeState(State::CONNECTING_API);
                break;
            }
            // Start recording immediately without waiting for WiFi
            audioPlayer.play("/button-press.wav");
            screen.displayMessage("[SYS] Recording...");
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
        } else if (maxRecordingTimer.isElapsed())
        {
            changeState(State::RECORDED);
        } else {
            break; // BREAK is conditionnal because we want to execute directly the next case "RECORDED" directly
        }
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
            waitingAiTimeout.start();
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
        if (waitingAiTimeout.isElapsed()) {
            screen.displayMessage("\n[SYS] Timeout recording stopped.");
            changeState(State::ERROR);
        }
        break;
    }
    case State::WAITING_AI_RESPONSE:
        if (runOnceOnStateChange())
        {
            screen.addMessage("\n[SYS] Waiting for AI\n");
            audioPlayer.play("/ai-begin.wav");
            // waitingAiTimeout.start();
        }
        // if (waitingAiTimeout.isElapsed()) {
        //     screen.displayMessage("\n[SYS] Timeout waiting for AI response.");
        //     changeState(State::ERROR);
        // }
        break;
    case State::PLAY_RESPONSE:
        if (runOnceOnStateChange())
        {
            afterAiResponseTimer.start();
            waitingAiTimeout.start();
        }
        if (!audioPlayer.isAudioPlaying() && audioPlayer.isStreamBufferEmpty() && afterAiResponseTimer.isElapsed())
        {
            changeState(State::FETCH_API_UPDATES);
            webSocket.disconnect();
            audioPlayer.play("/ai-end.wav");
        }
        if (waitingAiTimeout.isElapsed()) {
            screen.displayMessage("\n[SYS] Timeout playing AI response.");
            changeState(State::ERROR);
        }
        break;
    case State::ERROR:
        if (runOnceOnStateChange())
        {
            screen.addMessage("\nPress the button to reset.");
            enterSleepModeTimer.start();
        }
        if (isButtonPressed() && WAKE_UP_CAUSE != EspWakeUpCause::TIMER) {
            changeState(State::CONNECTING_WIFI);
        }
        if (enterSleepModeTimer.isElapsed()) 
        {
            changeState(State::SLEEP_MODE);
        }
        break;
    case State::BLE_PROVISIONING:
        if (runOnceOnStateChange())
        {
            if (WAKE_UP_CAUSE == EspWakeUpCause::TIMER) {
                changeState(State::ERROR);
                break;
            }
            screen.displayMessage("[SYS] BLE prov. mode: Use the app to connect to WiFi. Or press the button to retry.");
            WiFiProv.beginProvision(
                WIFI_PROV_SCHEME_BLE,
                WIFI_PROV_SCHEME_HANDLER_FREE_BTDM,
                WIFI_PROV_SECURITY_1,
                "abcd1234",
                "ESP32_Provisioning"
            );
            enterSleepModeTimer.start();
        }
        if (WiFi.status() == WL_CONNECTED) {
            changeState(State::CONNECTED_WIFI);
        }
        if (isButtonPressed()) {
            changeState(State::CONNECTING_WIFI);
        }
        if (enterSleepModeTimer.isElapsed()) 
        {
            changeState(State::SLEEP_MODE);
        }
        break;
    case State::SLEEP_MODE:
        if (runOnceOnStateChange())
        {
            sleepTimeout.start();
            screen.displayMessage("[SYS] Entering sleep mode...");
        }
        if (sleepTimeout.isElapsed())
        {
            sleepTimeout.breakIt();
            if (WAKE_UP_CAUSE != EspWakeUpCause::TIMER) {
                screen.clear();
            }
            powerController.startSleep(isAPIConnectionSuccessful);
        }
        break;
    case State::ALARM_MODE:
        if (runOnceOnStateChange())
        {
            screen.displayMessage("[SYS] Alarm mode: Press the button to stop the alarm.");
            audioPlayer.play("/alarm.wav");
            alarmTimeout.start();
        }
        if (!audioPlayer.isAudioPlaying() && audioPlayer.isStreamBufferEmpty()) {
            audioPlayer.play("/alarm.wav");
        }
        if (alarmOverDelay.isElapsed()) {
            audioPlayer.stop();
            changeState(State::IDLE);
            break;
        } else if (isButtonPressed() && alarmTimeout.breakIt()) {
            screen.addMessage("[SYS] Alarm stopped. Returning to IDLE.");
            audioPlayer.stop();
            alarmOverDelay.start();
        } else if (alarmTimeout.isElapsed()) {
            screen.addMessage("[SYS] Alarm timeout.");
            audioPlayer.stop();
            changeState(State::SLEEP_MODE);
        }
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
        // --- Binary frame: PCM audio chunk from TTS ---
        if (message.isBinary()) {
            const std::string& data = message.rawData();
            audioPlayer.pushStream(
                reinterpret_cast<const uint8_t*>(data.data()),
                data.size()
            );
            return;
        }

        // --- Text frame: control signals or displayable text ---
        std::string api_message = message.data().c_str();
        if (api_message.empty()) {
            return;
        } else if (api_message == ENV::TRANSCRIPTION_START) {
            screen.addMessage("\n[SYS] Transcribing...\n");
        } else if (api_message == ENV::TRANSCRIPTION_END) {
            // nothing to do
        } else if (api_message == ENV::AI_TEXT_START || api_message == ENV::AI_TTS_START) {
            if (getState() != State::WAITING_AI_RESPONSE) {
                changeState(State::WAITING_AI_RESPONSE);
            }
        } else if (api_message == ENV::AI_TEXT_END || api_message == ENV::AI_TTS_END) {
            if (api_message == ENV::AI_TTS_END) {
                // Push sentinel so PLAY_RESPONSE knows when all audio chunks are played
                audioPlayer.endStream();
            }
            ai_text_finished = ai_text_finished || (api_message == ENV::AI_TEXT_END);
            ai_tts_finished  = ai_tts_finished  || (api_message == ENV::AI_TTS_END);
            if (ai_text_finished && ai_tts_finished) {
                changeState(State::PLAY_RESPONSE);
                ai_text_finished = false;
                ai_tts_finished  = false;
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
            // changeState(State::CONNECTED_WIFI);
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
            Serial.println("[WiFi] Disconnected unexpectedly");
            changeState(State::ERROR);
            screen.displayMessage("[SYS] Error: WiFi disconnected.");
            break;
        case ARDUINO_EVENT_PROV_END:
            Serial.println("[WiFiProv] Ended");
            break;
        }
    });
}

void initComponents() {
    screen.init();
    if (!audioPlayer.init()) {
        Serial.println("AudioPlayer initialization failed");
        screen.displayMessage("[SYS] AudioPlayer initialization failed");
        changeState(State::ERROR);
    }
    audioPlayer.setVolume(15);
    audioPlayer.startStream();
}
