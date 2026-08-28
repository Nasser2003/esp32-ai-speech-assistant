
class SinusPulse {
public:
    SinusPulse(int frequencyHz = 1, int offsetDegree = 270);
    void startPulse();
    void stopPulse();
    int getPulseState();
    void setSinusPulse(int frequencyHz, int offsetDegree);
private:
    unsigned long pulseStartTime;
    int pulseFrequency;
    float pulseOffset;
    bool isPulsing;
};