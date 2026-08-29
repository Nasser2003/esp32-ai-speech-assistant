enum class State {
    PRE_INIT,       // Used to initialize the value of "lastState", so that the first state change can be detected
    INIT,           // When esp POWERED ON, or RESET
    CONNECTING_WIFI, // When esp is trying to connect to the wifi network
    CONNECTED_WIFI,  // When esp is connected to the wifi network
    IDLE,           // When esp is waiting for user input (button press)
    RECORDING,      // When esp is recording audio segments, sending to the api, and display the transcription on the screen
    RECORDED,       // When esp has finished recording audio (button released)
    WAITING_AI_RESPONSE, // When esp is waiting for a response from the server
    PLAYING_SHOWING,        // When esp is playing the audio response from the server
};
enum class RecordingState {
    INIT, // Default state when recording starts
    SAVING,
    SENDING_TO_API,
    WAITING_FOR_RESPONSE,
    DISPLAYING_TRANSCRIPTION,
};