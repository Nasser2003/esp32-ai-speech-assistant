#include "websocket-controller.h"


WebsocketController::WebsocketController(
    const char* host, 
    uint16_t port, 
    const char* path, 
    const char* RECORDING_START, 
    const char* RECORDING_END,
    const char* WEBSOCKET_CLOSE
)
: host(host), 
port(port), 
path(path), 
RECORDING_START(RECORDING_START), 
RECORDING_END(RECORDING_END),
WEBSOCKET_CLOSE(WEBSOCKET_CLOSE) {}

bool WebsocketController::connect()
{
    // Connect to the WebSocket server.
    return client.connect(host, port, path);
}

void WebsocketController::disconnect()
{
    // Disconnect from the WebSocket server.
    client.send(WEBSOCKET_CLOSE);
    client.close();
}

bool WebsocketController::startAudioSession()
{
    Serial.print("[WebsocketController] Starting audio session: ");
    Serial.println(RECORDING_START);

    return client.send(RECORDING_START);
}

void WebsocketController::update()
{
    // Process incoming and outgoing WebSocket events.
    client.poll();
}

bool WebsocketController::sendAudio(const uint8_t* data, size_t size)
{
    Serial.println("[WebsocketController] Sending audio data...");
    if (data == nullptr || size == 0) {
        return false;
    }
    Serial.println("[WebsocketController] Audio data sent successfully.");
    
    // Send the audio buffer as binary data.
    return client.sendBinary(
        reinterpret_cast<const char*>(data),
        size
    );
}

void WebsocketController::setMessageCallback(const websockets::MessageCallback& callback) {
    client.onMessage(callback);
}

bool WebsocketController::endAudioSession()
{
    Serial.print("[WebsocketController] Ending audio session: ");
    Serial.println(RECORDING_END);

    return client.send(RECORDING_END);
}