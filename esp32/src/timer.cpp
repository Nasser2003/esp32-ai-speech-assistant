#include <Arduino.h>
#include "timer.h"

void Timer::start(uint32_t durationMs) {
    if (durationMs == -1) {
        durationMs = duration;   // utilise la valeur membre
    }

    startTime = millis();
    duration = durationMs;
}

bool Timer::isElapsed() const
{
    if (startTime == 0) {
        return false; // Timer not started
    }
    return millis() - startTime >= duration;
}