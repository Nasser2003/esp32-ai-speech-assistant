#pragma once

#include <Arduino.h>

struct LocationTime
{
    String location;
    String timezone;

    float latitude;
    float longitude;

    bool valid;
};

class LocationTimeApi
{
public:
    explicit LocationTimeApi(uint32_t expirationMs = 24 * 60 * 60 * 1000UL); // Default expiration: 24 hours

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
    uint32_t lastFetchMs;

    bool saveToNvs();
    bool loadFromNvs();

    bool fetchLocation();
};