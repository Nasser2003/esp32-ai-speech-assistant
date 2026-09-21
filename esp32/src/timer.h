#include <Arduino.h>

enum class TimerState {
    NOT_STARTED,
    RUNNING,
    ELAPSED,
    BROKEN
};

class Timer {
public:
    Timer(uint32_t durationMs) : startTime(0), duration(durationMs), initialDuration(durationMs) {}

    void setDuration(uint32_t durationMs);

    void reset();

    void start(uint32_t durationMs = -1);

    bool isNotStarted() const;

    bool isRunning() const;

    bool isElapsed() const; // Returns true if timer elapsed, or broken, false if not started or still running

    bool isBroken() const; // Returns true if timer is broken

    bool breakIt(); // Useful to execute a condition once when "isElapsed" is validated

private:
    uint32_t startTime;
    uint32_t duration;
    const uint32_t initialDuration;

    TimerState getState() const;
};