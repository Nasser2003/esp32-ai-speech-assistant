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
    LocationTimeApi();

    bool begin();

    bool update();

    const LocationTime& get() const;

private:
    LocationTime locationTime;

    bool fetchLocation();
    bool syncTime();

    String timezoneToPosix(const String& timezone);
};