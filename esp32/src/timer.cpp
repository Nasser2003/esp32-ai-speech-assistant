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

bool Timer::isNotStarted() const {
    return getState() == TimerState::NOT_STARTED;
}

bool Timer::isBroken() const {
    return getState() == TimerState::BROKEN;
}

bool Timer::isRunning() const {
    return getState() == TimerState::RUNNING;
}

bool Timer::isElapsed() const
{
    return getState() == TimerState::ELAPSED;
}

bool Timer::breakIt() {
    if (getState() == TimerState::BROKEN) {
        return false; // Timer already broken
    }
    startTime = UINT32_MAX;
    return true;
}

TimerState Timer::getState() const {
    if (startTime == 0) {
        return TimerState::NOT_STARTED;
    } else if (startTime == UINT32_MAX) {
        return TimerState::BROKEN;
    } else if (millis() - startTime >= duration) {
        return TimerState::ELAPSED;
    } else {
        return TimerState::RUNNING;
    }
}