#include "location-time-api.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>

LocationTimeApi::LocationTimeApi(uint32_t expirationMs)
    : expirationMs(expirationMs), lastFetchMs(0)
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
    // 1. Try to use valid in-memory data
    if (locationTime.valid && !checkExpiration()) {
        return true;
    }

    // 2. Try to use valid persistent cache from NVS (without hitting HTTP API)
    if (loadFromNvs() && !checkExpiration()) {
        Serial.printf(
            "[LocationTime] Using persistent cache (loc: %s, tz: %s)\n",
            locationTime.location.c_str(),
            locationTime.timezone.c_str()
        );
        return true;
    }

    // 3. Cache not available or expired: do real HTTP request
    // If update() fails, it automatically falls back to saved data in NVS if available
    return update();
}

bool LocationTimeApi::checkExpiration()
{
    if (!locationTime.valid) {
        if (!loadFromNvs()) {
            return true; // Nothing in cache
        }
    }

    uint32_t nowMs = millis();
    uint32_t elapsedMs = nowMs - lastFetchMs;

    if (lastFetchMs != 0 && elapsedMs >= expirationMs) {
        Serial.printf(
            "[LocationTime] Cache expired (%u ms >= %u ms). Will refresh on next update.\n",
            elapsedMs,
            expirationMs
        );
        return true;
    }

    if (lastFetchMs != 0) {
        Serial.printf(
            "[LocationTime] Cache valid (remaining: %u s)\n",
            (expirationMs - elapsedMs) / 1000
        );
    }
    return false;
}

void LocationTimeApi::clear()
{
    Preferences prefs;
    if (!prefs.begin("loc_time", false)) {
        prefs.clear();
        prefs.end();
    }

    lastFetchMs = 0;

    locationTime.valid = false;
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
    prefs.putBool("valid", true);
    prefs.end();

    Serial.printf("[LocationTime] Saved to NVS (loc: %s, tz: %s)\n", locationTime.location.c_str(), locationTime.timezone.c_str());
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
    prefs.end();

    if (locationTime.location.isEmpty() || locationTime.timezone.isEmpty()) {
        locationTime.valid = false;
        return false;
    }

    locationTime.valid = true;
    if (lastFetchMs == 0) {
        lastFetchMs = millis();
    }
    return true;
}

bool LocationTimeApi::update()
{
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[LocationTime] WiFi not connected");
        if (locationTime.valid || loadFromNvs()) {
            Serial.printf(
                "[LocationTime] WiFi offline, using saved data from memory (loc: %s, tz: %s)\n",
                locationTime.location.c_str(),
                locationTime.timezone.c_str()
            );
            return true;
        }
        return false;
    }

    if (!fetchLocation()) {
        Serial.println("[LocationTime] API request failed, checking for saved data in memory...");
        if (locationTime.valid || loadFromNvs()) {
            Serial.printf(
                "[LocationTime] Fallback to saved data in memory (loc: %s, tz: %s)\n",
                locationTime.location.c_str(),
                locationTime.timezone.c_str()
            );
            return true;
        }
        Serial.println("[LocationTime] No saved data available in memory");
        return false;
    }

    locationTime.valid = true;
    lastFetchMs = millis();

    // Save to persistent storage (NVS)
    saveToNvs();

    Serial.println("========== Location / Timezone ==========");
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

    Serial.println("=========================================");

    return true;
}

bool LocationTimeApi::fetchLocation()
{
    HTTPClient http;

    // const char* url = "https://ipapi.co/json/";
    const char* url = "http://ip-api.com/json/";

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

    if ((document["error"] | false) || String(document["status"] | "") == "fail") {
        Serial.printf(
            "[LocationTime] API returned an error: %s\n",
            (const char*)(document["message"] | document["reason"] | "unknown")
        );
        return false;
    }

    // Temporary variables to avoid corrupting current data if incomplete
    String city = document["city"] | "";
    String region = document["regionName"] | (document["region"] | "");
    String country = document["countryCode"] | (document["country_code"] | "");
    String tz = document["timezone"] | "";
    float lat = document["lat"] | (document["latitude"] | 0.0f);
    float lon = document["lon"] | (document["longitude"] | 0.0f);

    if (tz.isEmpty()) {
        Serial.println("[LocationTime] Timezone missing");
        return false;
    }

    locationTime.location = city + ", " + region + ", " + country;
    locationTime.timezone = tz;
    locationTime.latitude = lat;
    locationTime.longitude = lon;

    return true;
}

const LocationTime& LocationTimeApi::get() const
{
    return locationTime;
}