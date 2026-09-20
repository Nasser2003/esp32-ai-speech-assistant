#pragma once

#include <Arduino.h>

struct Task
{
    int id;
    String taskType;
    String runAt;
    String argument;
    bool hasArgument;
};

class HttpController
{
public:
    HttpController(const char* host, uint16_t port);

    int getCurrentTask(Task& task);
    int updateTask();

private:
    String buildUrl(const char* path) const;
    Task currentTask;

    const char* host;
    uint16_t port;

    int get(const char* path, String& response);
};