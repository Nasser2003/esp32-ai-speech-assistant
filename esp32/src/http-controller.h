#pragma once

#include <Arduino.h>

enum class TaskStatus {
    PENDING,
    COMPLETED,
    CANCELLED
};

enum class TaskType {
    ALARM,
    WAKE_UP_AI,
    CHANGE_VOLUME,
    UNKNOWN
};

TaskType taskTypeToEnum(String type);
String taskTypeToString(TaskType type);

struct Task
{
    int id;
    TaskType type;
    String runAt;
    String argument;
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