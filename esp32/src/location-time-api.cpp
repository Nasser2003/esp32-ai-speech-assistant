#include "location-time-api.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <time.h>

LocationTimeApi::LocationTimeApi()
{
    locationTime.valid = false;
    locationTime.latitude = 0.0f;
    locationTime.longitude = 0.0f;
}

bool LocationTimeApi::begin()
{
    if (WiFi.status() != WL_CONNECTED) {
        // Serial.println("[LocationTime] WiFi not connected");
        return false;
    }

    return update();
}

bool LocationTimeApi::update()
{
    if (WiFi.status() != WL_CONNECTED) {
        // Serial.println("[LocationTime] WiFi not connected");
        return false;
    }

    if (!fetchLocation()) {
        return false;
    }

    if (!syncTime()) {
        return false;
    }

    struct tm timeInfo;

    if (!getLocalTime(&timeInfo, 5000)) {
        Serial.println("[LocationTime] Failed to get local time");
        return false;
    }

    char dateTime[20];

    strftime(
        dateTime,
        sizeof(dateTime),
        "%Y-%m-%d %H:%M:%S",
        &timeInfo
    );

    locationTime.dateTime = dateTime;

    locationTime.valid = true;

    Serial.println("========== Location / Time ==========");
    Serial.printf(
        "Date/time : %s\n",
        locationTime.dateTime.c_str()
    );

    Serial.printf(
        "Location  : %s\n",
        locationTime.location.c_str()
    );

    Serial.printf(
        "Timezone  : %s\n",
        locationTime.timezone.c_str()
    );

    Serial.printf(
        "Latitude  : %.6f\n",
        locationTime.latitude
    );

    Serial.printf(
        "Longitude : %.6f\n",
        locationTime.longitude
    );

    Serial.println("=====================================");

    return true;
}

bool LocationTimeApi::fetchLocation()
{
    HTTPClient http;

    const char* url = "https://ipapi.co/json/";

    if (!http.begin(url)) {
        Serial.println("[LocationTime] HTTP begin failed");
        return false;
    }

    http.setTimeout(5000);

    const int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf(
            "[LocationTime] HTTP error: %d\n",
            httpCode
        );

        http.end();
        return false;
    }

    const String payload = http.getString();

    http.end();

    JsonDocument document;

    const DeserializationError error =
        deserializeJson(document, payload);

    if (error) {
        Serial.printf(
            "[LocationTime] JSON error: %s\n",
            error.c_str()
        );

        return false;
    }

    if (document["error"] | false) {
        Serial.println("[LocationTime] API returned an error");
        return false;
    }

    locationTime.location =
        String(document["city"] | "") +
        ", " +
        String(document["region"] | "") +
        ", " +
        String(document["country_code"] | "");

    locationTime.timezone =
        document["timezone"] | "";

    locationTime.latitude =
        document["latitude"] | 0.0f;

    locationTime.longitude =
        document["longitude"] | 0.0f;

    if (locationTime.timezone.isEmpty()) {
        Serial.println("[LocationTime] Timezone missing");
        return false;
    }

    return true;
}

bool LocationTimeApi::syncTime()
{
    /*
     * ipapi returns an IANA timezone such as:
     *
     * Europe/Brussels
     * America/New_York
     * Asia/Tokyo
     *
     * ESP32 configTzTime() expects a POSIX TZ string.
     *
     * For now, use the timezone returned by the API
     * through a small mapping.
     */

    const String& timezone = locationTime.timezone;

    String posixTimezone =
        timezoneToPosix(timezone);

    if (posixTimezone.isEmpty()) {
        Serial.printf(
            "[LocationTime] Unsupported timezone: %s\n",
            timezone.c_str()
        );

        return false;
    }

    configTzTime(
        posixTimezone.c_str(),
        "pool.ntp.org",
        "time.nist.gov"
    );

    struct tm timeInfo;

    if (!getLocalTime(&timeInfo, 10000)) {
        Serial.println("[LocationTime] NTP synchronization failed");
        return false;
    }

    return true;
}

String LocationTimeApi::timezoneToPosix(
    const String& timezone
)
{
    /*
     * Add mappings as needed.
     *
     * Belgium / France / Germany / Netherlands:
     * CET-1CEST,M3.5.0/2,M10.5.0/3
     */

    if (
        timezone == "Europe/Brussels" ||
        timezone == "Europe/Paris" ||
        timezone == "Europe/Berlin" ||
        timezone == "Europe/Amsterdam" ||
        timezone == "Europe/Luxembourg"
    ) {
        return "CET-1CEST,M3.5.0/2,M10.5.0/3";
    }

    if (timezone == "Europe/London") {
        return "GMT0BST,M3.5.0/1,M10.5.0";
    }

    if (timezone == "UTC") {
        return "UTC0";
    }

    return "";
}

const LocationTime& LocationTimeApi::get() const
{
    return locationTime;
}