#include "power-controller.h"
#include <Arduino.h>
#include <algorithm>
#include <map>
#include <vector>
#include "esp_sleep.h"
namespace {
    constexpr int BATTERY_DIVIDER_RATIO = 2;
    // Measurements are grouped into 10 mV buckets.
    // Example: 2045-2054 mV -> bucket 2050
    constexpr int VOLTAGE_BUCKET_SIZE = 10;
    // Keep the measurements belonging to the most frequent
    // 50% of the history.
    constexpr float FREQUENCY_RATIO = 0.50f;
    // Button press threshold in millivolts.
    constexpr int BUTTON_PRESS_THRESHOLD = 1500;
}


PowerController::PowerController(int batteryPin, uint32_t periodicAwakeMS, int wakeUpPin,  int historySize)
    : batteryPin(batteryPin), 
    historySize(historySize), 
    periodicAwakeMS(periodicAwakeMS), 
    wakeUpPin(wakeUpPin) 
{
    // Initialize the deque with zeros
    for (int i = 0; i < historySize; ++i) {
        adcHistory.push_back(0);
    }
    // Get the wakeup cause
    sleepWakeUpCause = esp_sleep_get_wakeup_cause();
}

int PowerController::getBatteryPercentage() const
{
    // --------------------------------------------------
    // Remove invalid/uninitialized measurements
    // --------------------------------------------------
    std::vector<int> measurements;

    for (const int millivolts : adcHistory) {

        if (millivolts > 0) {
            measurements.push_back(millivolts);
        }
    }

    // No valid measurements available.
    if (measurements.empty()) {
        return 0.0f;
    }

    // --------------------------------------------------
    // Group measurements into voltage buckets
    // --------------------------------------------------
    std::map<int, int> occurrences;

    for (const int millivolts : measurements) {
        const int bucket =
            (millivolts / VOLTAGE_BUCKET_SIZE) *
            VOLTAGE_BUCKET_SIZE;
        occurrences[bucket]++;
    }


    // --------------------------------------------------
    // Sort buckets by frequency
    // --------------------------------------------------
    std::vector<std::pair<int, int>> sortedOccurrences(
        occurrences.begin(),
        occurrences.end()
    );

    std::sort(
        sortedOccurrences.begin(),
        sortedOccurrences.end(),
        [](
            const std::pair<int, int>& a,
            const std::pair<int, int>& b
        ) {
            return a.second > b.second;
        }
    );


    // --------------------------------------------------
    // Select measurements representing 80% of observations
    // --------------------------------------------------
    const size_t targetCount =
        static_cast<size_t>(
            measurements.size() *
            FREQUENCY_RATIO
        );

    size_t selectedCount = 0;
    long totalMillivolts = 0;


    for (const auto& occurrence : sortedOccurrences) {
        const int bucket = occurrence.first;
        const int count = occurrence.second;

        const size_t remaining =
            targetCount > selectedCount ? targetCount - selectedCount : 0;

        const int samplesToTake =
            std::min(count, static_cast<int>(remaining));


        // Use the center of the bucket as its representative value.
        totalMillivolts += static_cast<long>(bucket + VOLTAGE_BUCKET_SIZE / 2) * samplesToTake;

        selectedCount += samplesToTake;

        if (selectedCount >= targetCount) {
            break;
        }
    }


    // Safety check.
    if (selectedCount == 0) {
        return 0.0f;
    }


    // --------------------------------------------------
    // Calculate filtered ADC voltage
    // --------------------------------------------------
    const float gpioVoltage =
        static_cast<float>(totalMillivolts) /
        static_cast<float>(selectedCount);


    // --------------------------------------------------
    // Reconstruct battery voltage
    // --------------------------------------------------
    const float batteryVoltage = gpioVoltage * BATTERY_DIVIDER_RATIO;


    // --------------------------------------------------
    // Convert voltage to battery percentage
    // --------------------------------------------------
    return (int)((batteryVoltage - 3200) / 10.0f);
}

void PowerController::update()
{
    const int millivolts = readBatteryPercentage();

    adcHistory.push_back(millivolts);

    if (adcHistory.size() > historySize) {
        adcHistory.pop_front();
    }
}

int PowerController::readBatteryPercentage() const
{
    return analogReadMilliVolts(batteryPin);
}

void PowerController::startSleep(bool enableWakeupTimer) {
    // set wakeup timer
    if (enableWakeupTimer) {
        esp_sleep_enable_timer_wakeup((uint64_t) periodicAwakeMS * 1000ULL);
    }

    // set wakeup pin
    esp_deep_sleep_enable_gpio_wakeup(
        (1ULL << wakeUpPin),
        ESP_GPIO_WAKEUP_GPIO_LOW
    );

    Serial.flush();

    esp_deep_sleep_start();
}

EspWakeUpCause PowerController::getWakeUpCause() const {
    return static_cast<EspWakeUpCause>(sleepWakeUpCause);
}
