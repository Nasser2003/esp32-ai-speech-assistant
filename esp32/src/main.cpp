#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiProv.h>
#include <esp_wifi.h>
#include <wifi_provisioning/manager.h>
#include "wifi-credentials-manager.h"

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
#include "location-time-api.h"

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
WebsocketController webSocket(ENV::API_HOST, ENV::API_PORT, ENV::API_WEBSOCKET_PATH);
HttpController httpController(ENV::API_HOST, ENV::API_PORT);
PowerController powerController(ENV::BATTERY_PIN, ENV::WAKEUP_INTERVAL, ENV::BUTTON_PIN, 1000);
LocationTimeApi locationTimeApi(ENV::LOCATION_TIME_EXPIRATION_MS);

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
Timer alarmTimeout(5000);
Timer alarmOverDelay(1000);
Timer changeVolumeTimer(2000);

Timer updateTimer(0);

// Variables
static bool ai_text_finished = false;
static bool ai_tts_finished = false;
static bool isSleeping = false;
static EspWakeUpCause WAKE_UP_CAUSE;
// Varialbe stored in RTC memory (kept even after deep sleep). 
// If API connection was successful, allow periodic timer wake up for api update
RTC_DATA_ATTR bool isAPIConnectionSuccessful = false;
static Task currentTask;

// WiFi credentials manager (LRU-ordered, persisted in NVS)
// Using a pointer to avoid any potential global constructor issues on ESP32-C3
WifiCredentialsManager* credManager = nullptr;
static int _credIndex = 0; // current credential being tried in CONNECTING_WIFI

// Function declarations
void setWebSocketCallback();
void setWifiCallback();
bool isButtonPressed();
void initComponents();
String generateEsp32InfoJson(String type, int taskId);

void setup()
{
    Serial.begin(115200);

    delay(1000); // Useful to not skip the first Serial.print() after esp start

    // Heap-allocate the credentials manager to avoid any global-ctor issues
    credManager = new WifiCredentialsManager();

    // Load stored WiFi credentials from NVS
    credManager->load();

    // WiFi.setTxPower(WIFI_POWER_8_5dBm); // Set the WiFi transmission power to 8.5 dBm
    setWebSocketCallback();
    setWifiCallback();
    
    analogReadResolution(12); 
    analogSetPinAttenuation(ENV::BATTERY_PIN, ADC_11db); // Set the attenuation to 11 dB for the battery pin
    pinMode(ENV::BATTERY_PIN, INPUT);
    pinMode(ENV::BUTTON_PIN, INPUT_PULLUP);
    ledcSetup(0, 5000, 8); // to use PWM feature on the led
    ledcAttachPin(ENV::BLUE_LED, 0); // PWM way to setup pinMode
    WAKE_UP_CAUSE = powerController.getWakeUpCause();
}

void loop()
{
    switch (getState())
    {
    case State::PRE_INIT:
        if (runOnceOnStateChange()) 
        {
            if (WAKE_UP_CAUSE == EspWakeUpCause::GPIO) 
            {
                Serial.println("[WAKEUP] Wakeup cause: GPIO");
                // Speed up some timers to init esp faster after deep sleep
                preInitTimer.setDuration(0);
                initTimer.setDuration(0);
                initbatteryTimer.setDuration(0);
                preInitTimer.start();
                connectingTimer.reset();
                connectedWifiTimer.reset();
                enterSleepModeTimer.reset();
            } 
            else if (WAKE_UP_CAUSE == EspWakeUpCause::TIMER) 
            {
                Serial.println("[WAKEUP] Wakeup cause: TIMER");
                initTimer.setDuration(0);
                initbatteryTimer.setDuration(0);
                connectingTimer.setDuration(0);
                connectedWifiTimer.setDuration(0);
                enterSleepModeTimer.setDuration(0);
                changeState(State::CONNECTING_WIFI);
                updateTimer.start();
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
        break;
    case State::INIT:
        if (runOnceOnStateChange()) 
        {
            updateTimer.start();
            initComponents();
            locationTimeApi.checkExpiration();

            screen.displayMessage("Initializing...");
            
            initbatteryTimer.start();
            initTimer.start();
        }
        if (initbatteryTimer.isElapsed()) 
        {
            initbatteryTimer.breakIt();
            std::string batteryPercentage = std::to_string(powerController.getBatteryPercentage());
            screen.addMessage("\nBattery: " + batteryPercentage + "%");
        }
        if (initTimer.isElapsed()) 
        {
            changeState(State::CONNECTING_WIFI);
        }
        break;
    case State::CONNECTING_WIFI:
        if (runOnceOnStateChange()) 
        {
            _credIndex = 0;
            if (credManager == nullptr || credManager->count() == 0) {
                // No stored credentials — go straight to BLE provisioning
                changeState(State::BLE_PROVISIONING);
                break;
            }
            const auto& creds = credManager->getAll();
            // Try the first (most-recently-used) credential
            const WifiCredential& c = creds[_credIndex];
            screen.displayMessage(
                "Connecting to WiFi...\n" + c.ssid + " (" + std::to_string(_credIndex + 1) +
                "/" + std::to_string(creds.size()) + ")"
            );
            Serial.printf("[WiFi] Trying credential [%d]: %s\n", _credIndex, c.ssid.c_str());
            WiFi.begin(c.ssid.c_str(), c.password.c_str());
            connectingTimeout.start();
            connectingTimer.start();
        }
        if (WiFi.status() == WL_CONNECTED && connectingTimer.isElapsed()) 
        {
            // Promote successfully used credential to front of LRU list
            if (credManager != nullptr) {
                const auto& creds = credManager->getAll();
                if (_credIndex < (int)creds.size()) {
                    credManager->promote(creds[_credIndex].ssid);
                }
            }
            changeState(State::CONNECTED_WIFI);
        }
        if (connectingTimeout.isElapsed()) 
        {
            WiFi.disconnect();
            _credIndex++;
            const auto& creds = credManager->getAll();
            if (_credIndex < (int)creds.size()) {
                // Try the next credential
                const WifiCredential& c = creds[_credIndex];
                screen.displayMessage(
                    "Connecting to WiFi...\n" + c.ssid + " (" + std::to_string(_credIndex + 1) +
                    "/" + std::to_string(creds.size()) + ")"
                );
                Serial.printf("[WiFi] Trying credential [%d]: %s\n", _credIndex, c.ssid.c_str());
                WiFi.begin(c.ssid.c_str(), c.password.c_str());
                connectingTimeout.start();
            } else {
                // All credentials exhausted
                Serial.println("[WiFi] All credentials failed.");
                changeState(State::CONNECTION_FAILED);
            }
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
                Serial.printf("[STATE] Current task: %d %s (%s)\n",
                    task.id,
                    taskTypeToString(task.type).c_str(),
                    task.argument.c_str());
                currentTask = task;
            } else {
                Serial.printf("[STATE] No task available: %d\n", statusCode);
                changeState(State::IDLE);
                break;
            }

            // Initialize components to prepare for the task
            if (WAKE_UP_CAUSE == EspWakeUpCause::TIMER) {
                initComponents();
            }

            switch (task.type)
            {
            case TaskType::ALARM:
                // Changing the cause to make it functional at normal condition
                if (WAKE_UP_CAUSE == EspWakeUpCause::TIMER) {
                    WAKE_UP_CAUSE = EspWakeUpCause::GPIO;
                }
                changeState(State::ALARM_MODE);
                break;
            case TaskType::CHANGE_VOLUME:
                changeState(State::CHANGE_VOLUME);
                break;
            case TaskType::WAKE_UP_AI:
                changeState(State::AI_RINGSTONE);
                break;
            default:
                break;
            }

            if (WAKE_UP_CAUSE == EspWakeUpCause::TIMER) {
                changeState(State::SLEEP_MODE);
            } else {
                changeState(State::IDLE);
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
            audioPlayer.stop();   // clear ttsBusy/wavPlaying so play() works next time
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
            screen.displayMessage("[SYS] Recording...");
            // Play the press sound synchronously before activating the mic.
            // The ESP32-C3 has a single I2S port shared between mic (RX) and
            // speaker (TX) — playing and recording cannot overlap.
            audioPlayer.play("/button-press.wav");
            while (audioPlayer.isPlaying()) { vTaskDelay(1); }
            recorder.startRecording();
            blueLedPulse.startPulse();
            minRecordingTimer.start();
            maxRecordingTimer.start();

            webSocket.sendMessage(ENV::RECORDING_START);
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
            recorder.stopRecording();      // switches I2S back to TX
            audioPlayer.startStream();     // re-arm streamPlaying so TTS pushStream() works
            {
                // Send initial backpressure credit so API can start sending audio
                size_t freeBytes = audioPlayer.getFreeQueueBytes();
                std::string bufferSignal = std::string(ENV::BUFFER_FREE) + ":" + std::to_string(freeBytes);
                webSocket.sendMessage(bufferSignal.c_str());
            }
            audioPlayer.play("/button-release.wav");
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
                    String message = String(ENV::ARGUMENT) + ":" + generateEsp32InfoJson(ENV::RECORDING_END, -1);
                    webSocket.sendMessage(message.c_str());
                    webSocket.sendMessage(ENV::RECORDING_END);
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
            if (getLastState() != State::AI_RINGSTONE) {
                screen.addMessage("\n> ");
            }
            // ai-begin.wav only if the speaker is free (button-release may still be playing)
            if (!audioPlayer.isPlaying()) {
                audioPlayer.play("/ai-begin.wav");
            }
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
        // Wait for: queue empty + DMA fully drained (isStreamDrained) + short timer.
        // This avoids cutting off the tail of the last TTS audio chunk.
        if (!audioPlayer.isAudioPlaying()
            && audioPlayer.isStreamBufferEmpty()
            && audioPlayer.isStreamDrained()
            && afterAiResponseTimer.isElapsed())
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
            screen.addMessage("\nPress to reset.");
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
            screen.displayMessage(
                "[SYS] BLE prov. mode\nUse the app to add a WiFi network.\nStored: " +
                std::to_string(credManager ? credManager->count() : 0) + "/" +
                std::to_string(WifiCredentialsManager::MAX_CREDENTIALS)
            );
            // Clean up any previous provisioning session so beginProvision() can re-initialize cleanly
            wifi_prov_mgr_deinit();
            delay(100);

            // Stop WiFi auto-reconnect to prevent reconnect loop to old stored network
            WiFi.setAutoReconnect(false);
            WiFi.disconnect(false, false);
            delay(100);
            // reset_provisioned=true: clears WiFiProv's native WiFi NVS so it doesn't
            // skip BLE advertising with "Already Provisioned". Our custom "wifi_creds"
            // NVS namespace is separate and unaffected by this reset.
            // WIFI_PROV_SCHEME_HANDLER_NONE preserves Bluetooth RAM so BLE can restart.
            WiFiProv.beginProvision(
                WIFI_PROV_SCHEME_BLE,
                WIFI_PROV_SCHEME_HANDLER_NONE,
                WIFI_PROV_SECURITY_1,
                "abcd1234",           // proof of possession
                "ESP32_Provisioning", // BLE device name visible in the app
                nullptr,              // service_key (unused for security level 1)
                nullptr,              // custom UUID
                true                  // reset_provisioned: force BLE advertising even if already provisioned
            );
            enterSleepModeTimer.start();
        }
        if (WiFi.status() == WL_CONNECTED) {
            // BLE provisioning succeeded — read the new credential from esp_wifi
            // and accumulate it in our NVS manager (LRU)
            if (credManager != nullptr) {
                wifi_config_t wifiCfg;
                if (esp_wifi_get_config(WIFI_IF_STA, &wifiCfg) == ESP_OK) {
                    std::string ssid(reinterpret_cast<const char*>(wifiCfg.sta.ssid));
                    std::string pass(reinterpret_cast<const char*>(wifiCfg.sta.password));
                    if (!ssid.empty()) {
                        credManager->add(ssid, pass);
                        Serial.printf("[WiFiProv] Credential saved: %s\n", ssid.c_str());
                    }
                }
            }
            changeState(State::CONNECTED_WIFI);
            isAPIConnectionSuccessful = true;
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
        if (isButtonPressed() && WAKE_UP_CAUSE != EspWakeUpCause::TIMER) {
            changeState(State::CONNECTING_WIFI);
        }
        if (sleepTimeout.isElapsed())
        {
            sleepTimeout.breakIt();
            screen.clear();
            // if last connection was successful, during active mode, it will allow periodic wakeup
            powerController.startSleep(isAPIConnectionSuccessful);
        }
        break;
    case State::ALARM_MODE:
        if (runOnceOnStateChange())
        {
            screen.displayMessage("[SYS] Alarm mode: \nPress to stop.\n");
            audioPlayer.play("/alarm.wav");
            alarmTimeout.start();
        }
        if (alarmOverDelay.isElapsed()) {
            httpController.updateTask();
            changeState(State::IDLE);
        } else if (isButtonPressed() && alarmOverDelay.isNotStarted()) {
            audioPlayer.stop();
            screen.addMessage("[SYS] Alarm stopped. Returning to IDLE.");
            alarmOverDelay.start();
        } else if (alarmTimeout.isElapsed()) {
            screen.addMessage("[SYS] Alarm timeout.");
            audioPlayer.stop();
            changeState(State::SLEEP_MODE);
        } else if (!audioPlayer.isAudioPlaying() && alarmOverDelay.isNotStarted()) {
            audioPlayer.play("/alarm.wav");
        }
        break;
    case State::CHANGE_VOLUME:
        if (runOnceOnStateChange())
        {
            const char* volumeStr = currentTask.argument.c_str();
            screen.displayMessage("[SYS] Changing volume to: " + std::string(volumeStr) + "%");
            audioPlayer.setVolume(atoi(volumeStr));
            httpController.updateTask();
            changeVolumeTimer.start();
        }
        if (changeVolumeTimer.isElapsed()) {
            changeState(State::IDLE);
        }
        break;
    case State::AI_RINGSTONE:
        if (runOnceOnStateChange())
        {
            screen.displayMessage("[SYS] AI has a message for you!\nPress to listen.");
            audioPlayer.play("/ringstone-1.wav");
            alarmTimeout.start();
        }
        if (alarmOverDelay.isElapsed()) {
            if (!webSocket.connect()) {
                changeState(State::CONNECTING_API);
                break;
            }
            httpController.updateTask();
            String message = String(ENV::ARGUMENT) + ":" + generateEsp32InfoJson(ENV::AI_WAKE_UP, currentTask.id);
            webSocket.sendMessage(message.c_str());
            webSocket.sendMessage(ENV::AI_WAKE_UP);
            audioPlayer.startStream();
            {
                // Send initial backpressure credit so API can start sending audio
                size_t freeBytes = audioPlayer.getFreeQueueBytes();
                std::string bufferSignal = std::string(ENV::BUFFER_FREE) + ":" + std::to_string(freeBytes);
                webSocket.sendMessage(bufferSignal.c_str());
            }
            changeState(State::WAITING_AI_RESPONSE);
        } else if (isButtonPressed() && alarmOverDelay.isNotStarted()) {
            audioPlayer.stop();
            screen.displayMessage("[SYS] Calling... Waiting for AI.\n");
            alarmOverDelay.start();
        } else if (alarmTimeout.isElapsed()) {
            screen.addMessage("[SYS] Ringstone timeout.");
            audioPlayer.stop();
            changeState(State::SLEEP_MODE);
        } else if (!audioPlayer.isAudioPlaying() && alarmOverDelay.isNotStarted()) {
            audioPlayer.play("/ringstone-1.wav");
        }
        break;
    default:
        break;
    }

    // Update variables
    if (updateTimer.isElapsed()) { // I put true to skip the time for now 
        updateTimer.start();

        ledcWrite(0, blueLedPulse.getPulseState());
        recorder.update();
        screen.update();
        webSocket.update();
        powerController.update();

        // selecting some states to avoid breaking any logic or some processes in progress
        bool PRESSED_DURING_WAKEUP_BY_TIMER_SLEEP = 
            isButtonPressed() 
            && WAKE_UP_CAUSE == EspWakeUpCause::TIMER
            &&( getState() == State::CONNECTING_WIFI
            || getState() == State::CONNECTED_WIFI
            || getState() == State::FETCH_API_UPDATES
            || getState() == State::SLEEP_MODE);
        if (PRESSED_DURING_WAKEUP_BY_TIMER_SLEEP) {
            // WAKING UP IN GPIO MODE AS IF THE USER PRESSED THE BUTTON DURING SLEEP MODE
            WAKE_UP_CAUSE = EspWakeUpCause::GPIO;
            changeState(State::PRE_INIT);
        }
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
            // Backpressure: notify API of remaining free space in PCM queue
            size_t freeBytes = audioPlayer.getFreeQueueBytes();
            std::string bufferSignal = std::string(ENV::BUFFER_FREE) + ":" + std::to_string(freeBytes);
            client.send(bufferSignal.c_str());
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
        } else if (api_message == ENV::AI_WAKE_UP) {
            
        } else if (api_message == ENV::AI_TEXT_END || api_message == ENV::AI_TTS_END) {
            if (api_message == ENV::AI_TTS_END) {
                // Sentinel in the queue: pcmTask() clears ttsBusy when it processes it.
                // PLAY_RESPONSE then detects !isAudioPlaying() && isStreamBufferEmpty().
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
            screen.addMessage(api_message, true);
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
            if (getState() != State::ERROR) {
                screen.displayMessage("[SYS] Error: BLE provisioning failed : Credentials rejected.");
                changeState(State::ERROR);
            }
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
            State s = getState();
            // Ignore disconnect events during states that intentionally manage WiFi
            if (s == State::BLE_PROVISIONING || s == State::CONNECTING_WIFI ||
                s == State::PRE_INIT || s == State::INIT) {
                Serial.println("[WiFi] Disconnected (expected, ignoring)");
                break;
            }
            if (s != State::ERROR) {
                Serial.println("[WiFi] Disconnected unexpectedly");
                screen.displayMessage("[SYS] Error: WiFi disconnected.");
                changeState(State::BLE_PROVISIONING);
                break;
            }
        }
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
        return; // Do NOT call startStream() on a failed/uninitialized player
    }
    audioPlayer.startStream();
}

String generateEsp32InfoJson(String type,int taskId)
{
    JsonDocument document;

    document["type"] = type;
    document["volume"] = audioPlayer.getVolume();
    document["battery"] = powerController.getBatteryPercentage();

    if (locationTimeApi.begin()) {
        const LocationTime& info = locationTimeApi.get();

        document["date_time"] = info.dateTime;
        document["location"] = info.location;
        document["timezone"] = info.timezone;
    }

    if (taskId != -1) {
        document["task_id"] = taskId;
    }

    String json;
    serializeJson(document, json);

    return json;
}
