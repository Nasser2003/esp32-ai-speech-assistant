#include "http-controller.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

HttpController::HttpController(const char* host, uint16_t port)
    : host(host), port(port) {}

int HttpController::get(const char* path, String& response)
{
    response = "";

    if (path == nullptr || WiFi.status() != WL_CONNECTED) {
        return -1;
    }

    HTTPClient client;
    if (!client.begin(buildUrl(path))) {
        return -1;
    }

    client.setTimeout(2000);

    const int statusCode = client.GET();
    if (statusCode > 0) {
        response = client.getString();
    }

    client.end();
    return statusCode;
}

int HttpController::getCurrentTask(Task& task)
{
    String response;
    String macAddress = WiFi.macAddress();
    String path = "/current-task?mac_address=" + macAddress;
    const int statusCode = get(path.c_str(), response);
    if (statusCode <= 0) {
        return statusCode;
    }

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, response);
    if (error) {
        return -2;
    }

    if (document["error"].as<bool>()) {
        return -3;
    }

    task.id = document["id"] | 0;
    task.type = taskTypeToEnum(document["task_type"] | "");
    task.runAt = document["run_at"] | "";
    task.argument = document["argument"].as<String>();

    currentTask = task;
    return statusCode;
}

int HttpController::updateTask()
{
    if (currentTask.id <= 0 ||
        WiFi.status() != WL_CONNECTED) {
        return -1;
    }

    HTTPClient client;

    const String path = "/tasks/" + String(currentTask.id);

    if (!client.begin(buildUrl(path.c_str()))) {
        return -1;
    }

    client.setTimeout(2000);

    client.addHeader(
        "Content-Type",
        "application/json"
    );

    JsonDocument document;

    document["task_status"] = "COMPLETED";

    String body;

    serializeJson(document, body);

    const int statusCode = client.PATCH(body);

    client.end();

    return statusCode;
}

String HttpController::buildUrl(const char* path) const
{
    String normalizedPath = path;
    if (!normalizedPath.startsWith("/")) {
        normalizedPath = "/" + normalizedPath;
    }

    return "http://" + String(host) + ":" + String(port) + normalizedPath;
}

TaskType taskTypeToEnum(String type)
{
    if (type == "ALARM") {
        return TaskType::ALARM;
    } else if (type == "WAKE_UP_AI") {
        return TaskType::WAKE_UP_AI;
    } else if (type == "CHANGE_VOLUME") {
        return TaskType::CHANGE_VOLUME;
    } else {
        return TaskType::UNKNOWN; // Default to ALARM if unknown
    }
}

String taskTypeToString(TaskType type)
{
    switch (type) {
        case TaskType::ALARM:
            return "ALARM";
        case TaskType::WAKE_UP_AI:
            return "WAKE_UP_AI";
        case TaskType::CHANGE_VOLUME:
            return "CHANGE_VOLUME";
        default:
            return "UNKNOWN";
    }
}
