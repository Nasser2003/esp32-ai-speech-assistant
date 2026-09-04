#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string>

#include "oled-screen.h"

#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define OLED_ADDR 0x3C
#define OLED_RESET -1

static Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
enum ANIMATION_TYPE {
	OVERRIDE,
	COMPLETION
};

OledScreen128x32::OledScreen128x32(int SDA_PIN, int SCK_PIN, bool animated, int speed) : SDA_PIN(SDA_PIN), SCK_PIN(SCK_PIN), animated(animated), speed(speed) {
	currentMessage = "";
	lastMessage = "";
	textShowStartTime = 0;
	scrollOffset = 0;
	lastScrollTime = 0;
}

void OledScreen128x32::init() {
	Wire.begin(SDA_PIN, SCK_PIN);
	display.setRotation(2);
	display.setTextColor(SSD1306_WHITE);

	if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("OLED undetected !");
        while (true) {
            delay(1000);
        }
    }
}

void OledScreen128x32::displayMessage(std::string message) {
	// Serial.print("[OLED ORDER] Setting message: ");
	// Serial.println(message.c_str());
	this->lastMessage = this->currentMessage;
	this->currentMessage = message;
	this->textShowStartTime = millis(); // to sync the animation timer with the new message
	this->textShowEndTime = 0;
}

void OledScreen128x32::addMessage(std::string message) {
	// Serial.print("[OLED ORDER] Setting message: ");
	// Serial.println(message.c_str());
	uint32_t last_display_duration = this->textShowEndTime - this->textShowStartTime;
	this->textShowStartTime = millis();
	this->textShowEndTime = millis() + last_display_duration;
	this->lastMessage = this->currentMessage;
	this->currentMessage = this->currentMessage + message;
	// to sync the animation timer with the new message
	// this->textShowStartTime = millis() + (textShowEndTime - textShowStartTime);
}

void OledScreen128x32::update() {
	bool sameMessage = this->currentMessage == this->lastMessage;
	if (sameMessage) {
		return;
	}

	if (!this->animated) {
		showMessage(this->currentMessage);
		return;
	} else {
		int speedRation = this->speed / 25; // relative speed
		int frame = (millis() - this->textShowStartTime) / 100 * speedRation;
		
		int t1Len = this->lastMessage.length();
		int t2Len = this->currentMessage.length();

		ANIMATION_TYPE animationType = ANIMATION_TYPE::OVERRIDE;

		bool doesMessageContinue = currentMessage.rfind(lastMessage, 0) == 0;
		if (doesMessageContinue) {
			animationType = ANIMATION_TYPE::COMPLETION;
		}

		switch (animationType) {
		case ANIMATION_TYPE::OVERRIDE:
			if (t1Len > 20) {
				t1Len = 20;
			}
			if (frame <= t1Len) {
				showMessage(this->lastMessage.substr(0, t1Len - frame));
			} else if (frame <= t1Len + t2Len) {
				showMessage(this->currentMessage.substr(0, frame - t1Len));
			} else {
				this->lastMessage = this->currentMessage; // to avoid re-displaying the same message in the next update
				showMessage(this->currentMessage);
				textShowEndTime = millis();
			}
			break;
		case ANIMATION_TYPE::COMPLETION:
			if (frame <= (t2Len - t1Len)) {
				showMessage(this->currentMessage.substr(0, t1Len + frame));
			} else {
				this->lastMessage = this->currentMessage; // to avoid re-displaying the same message in the next update
				showMessage(this->currentMessage);
				textShowEndTime = millis();
			}
			break;
		}
	}
}

void OledScreen128x32::showMessage(const std::string& message) {
	std::string normalized = normalizeText(message);
	// If the screen has to show more than 4 lines, it will scroll the text up to show the last 4 lines
	bool sameMessage = normalized == this->currentDisplayedMessage;
	if (sameMessage) {
		return;
	}
	lastDisplayedMessage = currentDisplayedMessage;
	currentDisplayedMessage = normalized;
	
	display.clearDisplay();
	display.setCursor(0, 0);
	display.print(normalized.c_str());
	int16_t finalY = display.getCursorY();
	int totalLines = (finalY / 8) + 1;
	const int maxLines = 4;

	if (totalLines > maxLines) {
        scrollOffset = (totalLines - maxLines) * 8;
    } else {
        scrollOffset = 0;
    }
    display.clearDisplay();
    display.setCursor(0, -scrollOffset);
    display.print(normalized.c_str());

	display.display();
}

std::string OledScreen128x32::normalizeText(const std::string& text) {
    std::string result;

    for (size_t i = 0; i < text.length(); i++) {
        unsigned char c = text[i];

        // ASCII
        if (c < 128) {
            result += c;
            continue;
        }

        // UTF-8 : caractères accentués français
        if (c == 0xC3 && i + 1 < text.length()) {
            unsigned char next = text[++i];

            switch (next) {
                case 0xA0: // à
                case 0xA2: // â
                case 0xA4: // ä
                    result += 'a';
                    break;

                case 0xA7: // ç
                    result += 'c';
                    break;

                case 0xA8: // è
                case 0xA9: // é
                case 0xAA: // ê
                case 0xAB: // ë
                    result += 'e';
                    break;

                case 0xAE: // î
                case 0xAF: // ï
                    result += 'i';
                    break;

                case 0xB4: // ô
                case 0xB6: // ö
                    result += 'o';
                    break;

                case 0xB9: // ù
                case 0xBB: // û
                case 0xBC: // ü
                    result += 'u';
                    break;

                case 0xBF: // ÿ
                    result += 'y';
                    break;

                default:
                    result += '?';
                    break;
            }
        }
    }

    return result;
}

// EXAMPLE
// constexpr int SDA_PIN = 15;
// constexpr int SCK_PIN = 7;
// constexpr int BLUE_LED = 14;
// OledScreen128x32 screen(SDA_PIN, SCK_PIN);

// void setup() {
//     Serial.begin(115200);
//     delay(1000);
//	   screen.init();
//     
//     pinMode(BLUE_LED, OUTPUT);
//     digitalWrite(BLUE_LED, LOW);
// }

// void loop() {
//     screen.displayMessage("abcdefghijklmnopqrstuvwxyz123456789");
//     digitalWrite(BLUE_LED, HIGH);
//     delay(1000);

//     digitalWrite(BLUE_LED, LOW);
//     delay(1000);
// }