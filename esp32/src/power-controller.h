#pragma once
#include <deque>
#include "esp_sleep.h"

enum class EspWakeUpCause {
    RESET = ESP_SLEEP_WAKEUP_UNDEFINED,
    TIMER = ESP_SLEEP_WAKEUP_TIMER,
    GPIO = ESP_SLEEP_WAKEUP_GPIO,
};

class PowerController{
public:
    PowerController(int batteryPin, uint32_t periodicAwakeMS, int wakeUpPin, int historySize = 100);
    int getBatteryPercentage() const;
    void update();
    EspWakeUpCause getWakeUpCause() const;
    void startSleep(bool enableWakeupTimer = true); // Start deep sleep with optional wakeup timer
private:
    int readBatteryPercentage() const;
    std::deque<int> adcHistory;
    const int batteryPin;
    const int historySize;
    const uint32_t periodicAwakeMS;
    esp_sleep_source_t sleepWakeUpCause;
    const int wakeUpPin; // GPIO0 is used for wakeup from deep sleep
};