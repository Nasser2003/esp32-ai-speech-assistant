#pragma once
class OledScreen128x32 {
public:
    OledScreen128x32(int SDA_PIN, int SCK_PIN, bool animated = false, int speed = 100);
    void init();
    void displayMessage(std::string message);
    void update();
private:
    int SDA_PIN;
    int SCK_PIN;
    bool animated;
    int speed;
    std::string currentMessage = "";
    std::string lastMessage = "";
    uint32_t textShowStartTime = 0; 
};