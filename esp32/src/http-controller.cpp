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
    const int statusCode = get("/current-task", response);
    if (statusCode <= 0) {
        return statusCode;
    }

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, response);
    if (error) {
        return -2;
    }

    task.taskType = document["task_type"] | "";
    task.runAt = document["run_at"] | "";
    task.hasArgument = !document["argument"].isNull();
    task.argument = task.hasArgument ? document["argument"].as<String>() : "";
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