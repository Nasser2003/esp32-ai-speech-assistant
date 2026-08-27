#include <Arduino.h>
#include <LiquidCrystal.h>

class LcdController {
public:
    LcdController(LiquidCrystal* lcd, int lightPin = -1);
    void init();
    void displayMessage(const char* message);
    void setLuminosity(int level); // value from 0 to 255
    void clearDisplay();
private:
    LiquidCrystal* lcd;
    int lightPin;
};