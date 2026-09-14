#pragma once
#include <deque>

class PowerController{
public:
    PowerController(int batteryPin, int historySize = 100);
    int getBatteryPercentage() const;
    void update();
private:
    int readBatteryPercentage() const;
    std::deque<int> adcHistory;
    const int batteryPin;
    const int historySize;
};