#include <Arduino.h>
#include "lcd-controller.h"
#include <LiquidCrystal.h>
#include <string>

LcdController::LcdController(LiquidCrystal* lcd, int lightPin) : lcd(lcd), lightPin(lightPin) {}

void LcdController::init() {
    lcd->begin(16, 2);
    // displayMessage("Initializing... Please wait");
}

void LcdController::displayMessage(const char* message) {
    lcd->clear();
    std::string msg = std::string(message);

    // fill remaining characters for the 16x02 lcd screen
    if (msg.length() < 32) msg += std::string(32 - msg.length(), ' ');

    // divide message into 2 lines
    std::string msg1 = msg.substr(0, 16);
    std::string msg2 = msg.substr(16, 16);

    lcd->setCursor(0, 0);
    lcd->print(msg1.c_str());

    lcd->setCursor(0, 1);
    lcd->print(msg2.c_str());
}

void LcdController::setLuminosity(int level) {
    analogWrite(lightPin, level);
}

void LcdController::clearDisplay() {
    lcd->clear();
}