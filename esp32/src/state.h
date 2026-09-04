enum class State {
    PRE_INIT,       // Used to initialize the value of "lastState", so that the first state change can be detected
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
    PLAYING_SHOWING,        // When esp is playing the audio response from the server
};
enum class RecordedState {
    SENDING_AUDIO,
    ENDING_AUDIO,
};