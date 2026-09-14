#pragma once

//----------------------------------------------------------
// ENUMS ---------------------------------------------------
//----------------------------------------------------------

enum class State {
    NONE,       // Used to initialize the value of "lastState", so that the first state change can be detected
    INIT,           // When esp POWERED ON, or RESET
    CONNECTING_WIFI, // When esp is trying to connect to the wifi network
    CONNECTED_WIFI,  // When esp is connected to the wifi network
    CONNECTION_FAILED,  // When esp failed to connect to the wifi network
    CONNECTING_API,  // When esp is trying to connect to the api
    CONNECTED_API,  // When esp is connected to the api
    IDLE,           // When esp is waiting for user input (button press)
    RECORDING,      // When esp is recording audio segments, sending to the api, and display the transcription on the screen
    RECORDED,       // When esp has finished recording audio (button released)
    WAITING_AI_RESPONSE, // When esp is waiting for a response from the server
    PLAY_RESPONSE,        // When esp is playing the audio response from the server
    ERROR,          // When esp encounters an error
    BLE_PROVISIONING,  // When esp is in BLE provisioning mode, waiting for the app to send the wifi credentials
    BATTERY_MEASURE, // When esp is measuring the battery percentage and displaying it on the screen
};
enum class RecordedState {
    SENDING_AUDIO,
    ENDING_AUDIO,
};

const char* stateToString(State state) {
    switch (state) {
    case State::NONE:       return "NONE";
    case State::INIT:           return "INIT";
    case State::CONNECTING_WIFI: return "CONNECTING_WIFI";
    case State::CONNECTED_WIFI:  return "CONNECTED_WIFI";
    case State::CONNECTION_FAILED:  return "CONNECTION_FAILED";
    case State::CONNECTING_API:  return "CONNECTING_API";
    case State::CONNECTED_API:  return "CONNECTED_API";
    case State::IDLE:           return "IDLE";
    case State::RECORDING:      return "RECORDING";
    case State::RECORDED:       return "RECORDED";
    case State::WAITING_AI_RESPONSE: return "WAITING_AI_RESPONSE";
    case State::PLAY_RESPONSE:        return "PLAY_RESPONSE";
    case State::ERROR:          return "ERROR";
    case State::BLE_PROVISIONING:  return "BLE_PROVISIONING";
    case State::BATTERY_MEASURE: return "BATTERY_MEASURE";
    default:                    return "UNKNOWN_STATE";
    }
}

//----------------------------------------------------------
// CLASS ---------------------------------------------------
//----------------------------------------------------------

State _state = State::NONE;
State _lastState = State::NONE;
RecordedState _recordedState = RecordedState::SENDING_AUDIO;
bool _isRunOnce = false; // Used to run code only once when the state changes, because the loop() function runs continuously

State getState() {
    return _state;
}

State getLastState() {
    return _lastState;
}

RecordedState getRecordedState() {
    return _recordedState;
}

void changeState(State newState) {
    _lastState = _state;
    _state = newState;
    Serial.printf("[STATE] Change: %s -> %s\n", 
        stateToString(_lastState), 
        stateToString(_state)
    );
    _isRunOnce = false;
}

void changeRecordedState(RecordedState newState) {
    _recordedState = newState;
}

static bool runOnceOnStateChange() {
    if (!_isRunOnce) {
        _isRunOnce = true;
        return true;
    }
    return false;
}
