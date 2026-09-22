#include "location-time-api.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>

// RTC memory survives deep sleep as long as power is maintained
RTC_DATA_ATTR static uint32_t rtcSavedEpoch = 0;

LocationTimeApi::LocationTimeApi(uint32_t expirationMs)
    : expirationMs(expirationMs), savedEpoch(0)
{
    locationTime.valid = false;
    locationTime.latitude = 0.0f;
    locationTime.longitude = 0.0f;
}

void LocationTimeApi::setExpiration(uint32_t ms)
{
    expirationMs = ms;
}

bool LocationTimeApi::begin()
{
    // 1. Try to use valid persistent cache (without hitting HTTP API)
    if (loadFromNvs()) {
        if (refreshTimeFromRtc()) {
            Serial.printf(
                "[LocationTime] Using persistent cache (time: %s, loc: %s)\n",
                locationTime.dateTime.c_str(),
                locationTime.location.c_str()
            );
            return true;
        } else {
            Serial.println("[LocationTime] Persistent cache expired or RTC lost");
            clear();
        }
    }

    // 2. Cache not available or expired: do real HTTP + NTP request
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[LocationTime] WiFi not connected, cannot fetch new data");
        return false;
    }

    return update();
}

bool LocationTimeApi::checkExpiration()
{
    if (!loadFromNvs()) {
        return true; // Nothing in cache
    }

    time_t now = time(nullptr);
    // If system time was reset (cold reboot before NTP), RTC is lost
    if (now < 1000000000) {
        Serial.println("[LocationTime] RTC time reset/invalid, clearing persistent cache");
        clear();
        return true;
    }

    int64_t elapsedSec = (int64_t)now - (int64_t)savedEpoch;
    uint64_t elapsedMs = (elapsedSec > 0) ? ((uint64_t)elapsedSec * 1000ULL) : UINT64_MAX;

    if (elapsedMs >= expirationMs || elapsedSec < 0) {
        Serial.printf(
            "[LocationTime] Cache expired (%llu s >= %u s). Clearing persistent data.\n",
            (unsigned long long)elapsedSec,
            expirationMs / 1000
        );
        clear();
        return true;
    }

    Serial.printf(
        "[LocationTime] Cache valid (age: %llu s, remaining: %llu s)\n",
        (unsigned long long)elapsedSec,
        (unsigned long long)((expirationMs / 1000) - elapsedSec)
    );
    return false;
}

void LocationTimeApi::clear()
{
    Preferences prefs;
    if (prefs.begin("loc_time", false)) {
        prefs.clear();
        prefs.end();
    }

    rtcSavedEpoch = 0;
    savedEpoch = 0;

    locationTime.valid = false;
    locationTime.dateTime = "";
    locationTime.location = "";
    locationTime.timezone = "";
    locationTime.latitude = 0.0f;
    locationTime.longitude = 0.0f;

    Serial.println("[LocationTime] Persistent data cleared");
}

bool LocationTimeApi::saveToNvs()
{
    Preferences prefs;
    if (!prefs.begin("loc_time", false)) {
        Serial.println("[LocationTime] Failed to open NVS for writing");
        return false;
    }

    prefs.putString("location", locationTime.location);
    prefs.putString("timezone", locationTime.timezone);
    prefs.putFloat("lat", locationTime.latitude);
    prefs.putFloat("lon", locationTime.longitude);
    prefs.putUInt("epoch", savedEpoch);
    prefs.putBool("valid", true);
    prefs.end();

    rtcSavedEpoch = savedEpoch;

    Serial.printf("[LocationTime] Saved to NVS (epoch: %u, loc: %s)\n", savedEpoch, locationTime.location.c_str());
    return true;
}

bool LocationTimeApi::loadFromNvs()
{
    Preferences prefs;
    if (!prefs.begin("loc_time", true)) {
        return false;
    }

    bool valid = prefs.getBool("valid", false);
    if (!valid) {
        prefs.end();
        return false;
    }

    locationTime.location = prefs.getString("location", "");
    locationTime.timezone = prefs.getString("timezone", "");
    locationTime.latitude = prefs.getFloat("lat", 0.0f);
    locationTime.longitude = prefs.getFloat("lon", 0.0f);
    savedEpoch = prefs.getUInt("epoch", 0);
    prefs.end();

    if (locationTime.location.isEmpty() || locationTime.timezone.isEmpty() || savedEpoch == 0) {
        locationTime.valid = false;
        return false;
    }

    locationTime.valid = true;
    return true;
}

bool LocationTimeApi::refreshTimeFromRtc()
{
    time_t now = time(nullptr);
    if (now < 1000000000 || savedEpoch == 0) {
        return false;
    }

    int64_t elapsedSec = (int64_t)now - (int64_t)savedEpoch;
    if (elapsedSec < 0 || ((uint64_t)elapsedSec * 1000ULL) >= expirationMs) {
        return false; // Expired
    }

    // Apply delay to saved base value: currentEpoch = savedEpoch + elapsedSec
    time_t currentEpoch = savedEpoch + elapsedSec;

    applyPosixTz(locationTime.timezone);

    struct tm timeInfo;
    if (!localtime_r(&currentEpoch, &timeInfo)) {
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
    return true;
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

    savedEpoch = time(nullptr);

    char dateTime[20];

    strftime(
        dateTime,
        sizeof(dateTime),
        "%Y-%m-%d %H:%M:%S",
        &timeInfo
    );

    locationTime.dateTime = dateTime;
    locationTime.valid = true;

    // Save to persistent storage (NVS + RTC)
    saveToNvs();

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

void LocationTimeApi::applyPosixTz(const String& timezone)
{
    String posix = timezoneToPosix(timezone);
    if (!posix.isEmpty()) {
        setenv("TZ", posix.c_str(), 1);
        tzset();
    }
}

String LocationTimeApi::timezoneToPosix(
    const String& timezone
)
{
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