#include <Arduino.h>
#include "audio-player.h"
#include "audio-recorder.h"
#include "oled-screen.h"
#include "sinus-pulse.h"
#include "timer.h"
#include "state.h"
#include "credentials.h"
#include <WiFi.h>

// Variables
AudioPlayer audioPlayer(39, 42, 3);
OledScreen128x32 screen(15, 7, true, 150);
AudioRecorder recorder(18,17,40);
SinusPulse blueLedPulse(1, 270);

// Timers
Timer preInitTimer(10);
Timer initTimer(2000);
Timer connected_wifi(1000);
Timer minRecordingTimer(2000);
Timer maxRecordingTimer(15000);
Timer recordStopTimer(1000);

Timer updateTimer(2);

// Constants
constexpr int BUTTON_PIN = 2;
constexpr int BLUE_LED = 14;

// State machine
State currentState = State::INIT;
State lastState = State::PRE_INIT;
RecordingState currentRecordingState = RecordingState::INIT;

// Function declarations
static bool isButtonPressed();
static bool executeOnlyOnceOnStateChange();

void setup()
{
    currentState = State::INIT;
    Serial.begin(115200);
    delay(1000);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    ledcSetup(0, 5000, 8); // to use PWM feature on the led
    ledcAttachPin(BLUE_LED, 0); // PWM way to setup pinMode
    screen.init();
    preInitTimer.start();
    WiFi.setTxPower(WIFI_POWER_8_5dBm); // Set the WiFi transmission power to 8.5 dBm to avoid the brownout effect
}

void loop()
{
    switch (currentState)
    {
    case State::INIT:
        if (preInitTimer.isElapsed() && executeOnlyOnceOnStateChange()) 
        {
            Serial.println("[INIT] Initializing...");
            screen.displayMessage("Initializing the AI assistant...");
            initTimer.start();
            updateTimer.start();
        }
        if (initTimer.isElapsed()) 
        {
            currentState = State::CONNECTING_WIFI;
        }
        break;
    case State::CONNECTING_WIFI:
        if (executeOnlyOnceOnStateChange()) 
        {
            Serial.println("[CONNECTING_WIFI] Connecting to WiFi...");
            screen.displayMessage("Connecting to WiFi...");
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
        if (WiFi.status() == WL_CONNECTED) 
        {
            currentState = State::CONNECTED_WIFI;
        }
        break;
    case State::CONNECTED_WIFI:
        if (executeOnlyOnceOnStateChange()) 
        {
            Serial.println("[CONNECTED_WIFI] Connected to WiFi!");
            screen.addMessage("\nConnected to WiFi!");
            connected_wifi.start();
        }
        if (connected_wifi.isElapsed()) 
        {
            currentState = State::IDLE;
        }
        break;
    case State::IDLE:
        if (executeOnlyOnceOnStateChange()) 
        {
            Serial.println("[IDLE] Waiting for user input (button press)");
            screen.displayMessage("Hold the button to record min 2 sec and max 15 sec.");
        }

        if (isButtonPressed()) // button pressed
        {
            currentState = State::RECORDING;
        }
        break;
    case State::RECORDING:
        if (executeOnlyOnceOnStateChange()) 
        {
            currentRecordingState = RecordingState::INIT;
            Serial.println("[RECORDING] Recording audio...");
            screen.displayMessage("Recording audio...");
            recorder.startRecording();
            blueLedPulse.startPulse();
            minRecordingTimer.start();
            maxRecordingTimer.start();
        }

        if (!isButtonPressed() && minRecordingTimer.isElapsed())
        {
            currentState = State::RECORDED;
        }
        if (maxRecordingTimer.isElapsed())
        {
            currentState = State::RECORDED;
        }
        break;
    case State::RECORDED:
    {
        if (executeOnlyOnceOnStateChange()) 
        {
            Serial.println("[RECORDED] Stopping recording...");
            screen.displayMessage("Stopping recording...");
            recorder.stopRecording();
            blueLedPulse.stopPulse();
            recordStopTimer.start();
        }
        if (recordStopTimer.isElapsed())
        {
            currentState = State::WAITING_AI_RESPONSE;
        }
        break;
    }
    case State::WAITING_AI_RESPONSE:
        if (executeOnlyOnceOnStateChange())
        {
            recorder.save("/recording.wav");
            Serial.println("[WAITING_AI_RESPONSE] Waiting for AI response...");
            screen.displayMessage("Waiting for AI response...");

            if (!audioPlayer.init()) {
                Serial.println("AudioPlayer initialization failed");
            }
            
            audioPlayer.setVolume(10);

            audioPlayer.play("/recording.wav");
        }
        break;
    default:
        break;
    }

    // Update variables
    if (true || updateTimer.isElapsed()) {
        updateTimer.start();

        ledcWrite(0, blueLedPulse.getPulseState());
        recorder.update();
        screen.update();
        audioPlayer.loop();
    }
    

    // if (buttonReleased && recorder.isRecordingState()) {
    //     recorder.stopRecording();
    //     recorder.save("/recording.wav");
    //     audioPlayer.setVolume(100);

    //     if (!audioPlayer.play("/recording.wav")) {
    //         Serial.println("Playback failed");
    //     }

    //     Serial.println("Recording saved");
    //     delay(50);
    // }

    // // Process audio while recording
    // recorder.update();
}

static bool executeOnlyOnceOnStateChange() {
    bool stateHasChanged = (currentState != lastState);
    if (stateHasChanged) {
        lastState = currentState;
        return true;
    }
    return false;
}

static bool isButtonPressed() {
    return digitalRead(BUTTON_PIN) == LOW;
}