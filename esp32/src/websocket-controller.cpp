#include "websocket-controller.h"

WebsocketController::WebsocketController(const char* host, uint16_t port, const char* path)
    : host(host), port(port), path(path) {}

bool WebsocketController::connect()
{
    // Connect to the WebSocket server.
    return client.connect(host, port, path);
}

void WebsocketController::disconnect()
{
    // Disconnect from the WebSocket server.
    client.close();
}

bool WebsocketController::startAudioSession()
{
    String message = "AUDIO STREAM START";
    
    Serial.print("[WebsocketController] Starting audio session: ");
    Serial.println(message);

    return client.send(message.c_str());
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
    String message = "AUDIO STREAM END";
    
    Serial.print("[WebsocketController] Ending audio session: ");
    Serial.println(message);

    return client.send(message.c_str());
}