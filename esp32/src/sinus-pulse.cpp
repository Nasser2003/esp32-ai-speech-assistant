#include <Arduino.h>
#include "sinus-pulse.h"

SinusPulse::SinusPulse(int frequencyHz, int offsetDegree) {
    pulseStartTime = 0;
    setSinusPulse(frequencyHz, offsetDegree);
    isPulsing = false;
}

void SinusPulse::startPulse() {
    pulseStartTime = millis();
    isPulsing = true;
}

void SinusPulse::stopPulse() {
    isPulsing = false;
}

int SinusPulse::getPulseState()
{
    if (!isPulsing) {
        return 0;
    }

    const unsigned long elapsedTime =
        millis() - pulseStartTime;

    const float phase =
        (2.0f * PI * pulseFrequency * elapsedTime) / 1000.0f;

    const float offset =
        pulseOffset * PI / 180.0f;

    const float sinusValue =
        (sin(phase + offset) + 1.0f) / 2.0f;

    return static_cast<int>(
        sinusValue * 255.0f
    );
}

void SinusPulse::setSinusPulse(int frequencyHz, int offsetDegree) {
    pulseFrequency = frequencyHz;
    pulseOffset = offsetDegree;
}