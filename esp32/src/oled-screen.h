#pragma once
class OledScreen128x32 {
public:
    OledScreen128x32(int SDA_PIN, int SCK_PIN, bool animated = false, int speed = 100);
    void init();
    void displayMessage(std::string message);
    void addMessage(std::string message);
    void update();
private:
    int SDA_PIN;
    int SCK_PIN;
    bool animated;
    int speed;
    std::string currentMessage = "";
    std::string lastMessage = "null"; // last message that was set, used to detect changes
    std::string currentDisplayedMessage = ""; // physically displayed message on the screen
    std::string lastDisplayedMessage = "null"; // physically displayed message on the screen
    uint32_t textShowStartTime = 0;
    uint32_t textShowEndTime = 0;

    int scrollOffset;
    uint32_t lastScrollTime;
    void showMessage(const std::string& message);
    std::string normalizeText(const std::string& text);
};