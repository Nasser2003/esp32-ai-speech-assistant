#pragma once

#include <Arduino.h>

struct Task
{
    String taskType;
    String runAt;
    String argument;
    bool hasArgument;
};

class HttpController
{
public:
    HttpController(const char* host, uint16_t port);

    int get(const char* path, String& response);
    int getCurrentTask(Task& task);

private:
    String buildUrl(const char* path) const;

    const char* host;
    uint16_t port;
};