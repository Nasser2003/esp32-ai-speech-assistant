#pragma once

#include <Arduino.h>
#include <ArduinoWebsockets.h>

class WebsocketController
{
public:
    WebsocketController(
        const char* host, 
        uint16_t port, 
        const char* path
    );

    bool connect();

    void disconnect();

    void update();

    bool sendMessage(const char* message);
    
    bool sendAudio(const uint8_t* data, size_t size);

    void setMessageCallback(const websockets::MessageCallback& callback);
private:
    websockets::WebsocketsClient client;
    const char* host;
    uint16_t port;
    const char* path;
    const char* RECORDING_START;
    const char* RECORDING_END;
};