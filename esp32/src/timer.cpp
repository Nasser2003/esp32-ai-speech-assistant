#include <Arduino.h>
#include "timer.h"
#include <cstdint>

void Timer::start(uint32_t durationMs) {
    if (durationMs == -1) {
        durationMs = duration;   // utilise la valeur membre
    }

    startTime = millis();
    duration = durationMs;
}

void Timer::setDuration(uint32_t durationMs) {
    duration = durationMs;
}

bool Timer::isElapsed() const
{
    if (startTime == UINT32_MAX) {
        return false; // Timer broken
    }

    if (startTime == 0) {
        return false; // Timer not started
    }
    return millis() - startTime >= duration;
}

bool Timer::breakIt() {
    if (startTime == UINT32_MAX) {
        return false; // Timer already broken
    }
    startTime = UINT32_MAX;
    return true;
}