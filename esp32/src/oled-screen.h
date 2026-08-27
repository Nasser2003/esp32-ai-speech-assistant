#pragma once

class OledScreen128x32 {
public:
    OledScreen128x32(int SDA_PIN, int SCK_PIN);
    void init();
    void displayMessage(std::string message);
private:
    int SDA_PIN;
    int SCK_PIN;
};