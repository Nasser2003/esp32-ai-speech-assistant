#include <Arduino.h>

class Timer {
public:
    Timer() : startTime(0), duration(0) {}
    Timer(uint32_t durationMs) : startTime(0), duration(durationMs) {}
    void start(uint32_t durationMs = -1);

    bool isElapsed() const;

private:
    uint32_t startTime = 0;
    uint32_t duration = 0;
};