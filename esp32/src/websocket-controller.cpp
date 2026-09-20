#include "websocket-controller.h"


WebsocketController::WebsocketController(
    const char* host, 
    uint16_t port, 
    const char* path
)
: host(host), 
port(port), 
path(path) {}

bool WebsocketController::connect()
{
    // Connect to the WebSocket server.
    return client.connect(host, port, path);
}

void WebsocketController::disconnect()
{
    client.close();
}

bool WebsocketController::sendMessage(const char* message)
{
    Serial.print("[WebsocketController] sending message: ");
    Serial.println(message);

    return client.send(message);
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