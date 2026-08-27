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

OledScreen128x32::OledScreen128x32(int SDA_PIN, int SCK_PIN) : SDA_PIN(SDA_PIN), SCK_PIN(SCK_PIN) {}

void OledScreen128x32::init() {
	Wire.begin(SDA_PIN, SCK_PIN);

	if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("OLED non détecté !");
        while (true) {
            delay(1000);
        }
    }
}

void OledScreen128x32::displayMessage(std::string message) {
	display.clearDisplay();
	display.setRotation(2);
	display.setCursor(0, 0);
	display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
	display.print(message.c_str());
	// display.println(" cm");
	display.display();
}


// EXAMPLE
// constexpr int SDA_PIN = 15;
// constexpr int SCK_PIN = 7;
// OledScreen128x32 screen(SDA_PIN, SCK_PIN);

// void setup() {
//     Serial.begin(115200);
//     delay(1000);
//     screen.init();
//     pinMode(14, OUTPUT);
//     digitalWrite(14, LOW);
// }

// void loop() {
//     screen.displayMessage("abcdefghijklmnopqrstuvwxyz123456789");
//     digitalWrite(14, HIGH);
//     delay(1000);

//     digitalWrite(14, LOW);
//     delay(1000);
// }