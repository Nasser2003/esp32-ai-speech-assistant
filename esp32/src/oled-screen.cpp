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

OledScreen128x32::OledScreen128x32(int SDA_PIN, int SCK_PIN, bool animated, int speed) : SDA_PIN(SDA_PIN), SCK_PIN(SCK_PIN), animated(animated), speed(speed) {
	currentMessage = "";
	lastMessage = "";
	textShowStartTime = 0;
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
	this->lastMessage = this->currentMessage;
	this->currentMessage = message;
	this->textShowStartTime = millis(); // to sync the animation timer with the new message
}

void OledScreen128x32::update() {
	bool sameMessage = this->currentMessage == this->lastMessage;
	if (sameMessage) {
		return;
	}
	if (!this->animated) {
		display.clearDisplay();
		display.setCursor(0, 0);
		display.print(currentMessage.c_str());
		display.display();
		return;
	} else {
		int speedRation = this->speed / 25; // relative speed
		int frame = (millis() - this->textShowStartTime) / 100 * speedRation;
		
		int t1Len = this->lastMessage.length();
		int t2Len = this->currentMessage.length();

		if (frame <= t1Len) {
			display.clearDisplay();
			display.setCursor(0, 0);
			display.print(this->lastMessage.substr(0, t1Len - frame).c_str());
			display.display();
		} else if (frame <= t1Len + t2Len) {
			display.clearDisplay();
			display.setCursor(0, 0);
			display.print(this->currentMessage.substr(0, frame - t1Len).c_str());
			display.display();
		} else {
			this->lastMessage = this->currentMessage; // to avoid re-displaying the same message in the next update
		}
	}
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