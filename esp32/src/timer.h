#include <Arduino.h>

class Timer {
public:
    Timer(uint32_t durationMs) : startTime(0), duration(durationMs) {}

    void setDuration(uint32_t durationMs);

    void start(uint32_t durationMs = -1);

    bool isElapsed() const; // Returns true if timer elapsed, or broken, false if not started or still running

    bool breakIt(); // Useful to execute a condition once when "isElapsed" is validated

private:
    uint32_t startTime;
    uint32_t duration;
};