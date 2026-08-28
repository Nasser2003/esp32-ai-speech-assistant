#include <Arduino.h>
#include "audio-player.h"
#include "audio-recorder.h"
#include "oled-screen.h"
#include "sinus-pulse.h"
#include "timer.h"
#include "state.h"

// Variables
AudioPlayer audioPlayer(39, 42, 3);
OledScreen128x32 screen(15, 7, true);
AudioRecorder recorder(18,17,40);
SinusPulse blueLedPulse(1, 270);

// Timers
Timer initTimer(2000);
Timer preInitTimer(1);
Timer minRecordingTimer(2000);
Timer maxRecordingTimer(15000);
Timer recordStopTimer(2000);

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
}

void loop()
{
    // audioPlayer.loop();
    // vTaskDelay(1);
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
            // recorder.startRecording();
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
            Serial.println("[WAITING_AI_RESPONSE] Waiting for AI response...");
            screen.displayMessage("Waiting for AI response...");
        }
        break;
    default:
        break;
    }

    // Update variables
    if (updateTimer.isElapsed()) {
        updateTimer.start();

        ledcWrite(0, blueLedPulse.getPulseState());
        recorder.update();
        screen.update();
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