#pragma once

#include <Arduino.h>

struct LocationTime
{
    String dateTime;
    String location;
    String timezone;

    float latitude;
    float longitude;

    bool valid;
};

class LocationTimeApi
{
public:
    explicit LocationTimeApi(uint32_t expirationMs = 10 * 60 * 1000);

    void setExpiration(uint32_t expirationMs);

    bool begin();

    bool update();

    // Checks whether persistent data has expired. If expired, clears it.
    // Call this in State::INIT.
    bool checkExpiration();

    void clear();

    const LocationTime& get() const;

private:
    uint32_t expirationMs;
    LocationTime locationTime;
    uint32_t savedEpoch;

    bool saveToNvs();
    bool loadFromNvs();
    bool refreshTimeFromRtc();

    bool fetchLocation();
    bool syncTime();

    String timezoneToPosix(const String& timezone);
    void applyPosixTz(const String& timezone);
};