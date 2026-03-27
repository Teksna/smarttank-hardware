#include "display.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans9pt7b.h>

Adafruit_SH1106G display(128, 64, &Wire, -1);

void initDisplay() {
    display.begin(0x3C, true);
    display.clearDisplay();
    display.display();
}

void displayStatus(int capacity)
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setFont(&FreeSans9pt7b);
    display.setCursor(6,15);

    // Tank outline
    display.drawLine(0, display.height(), 0, 11, SH110X_WHITE);
    display.drawLine(0, display.height()-1, 60, display.height()-1, SH110X_WHITE);
    display.drawLine(60, display.height(), 60, 11, SH110X_WHITE);
    display.drawLine(0, 11, 20, 1, SH110X_WHITE);
    display.fillRect(20, 0, 20, 3,1);
    display.drawLine(40, 1, 60, 11, SH110X_WHITE);
    display.drawLine(1, 11, 59, 11, SH110X_WHITE);

    // Header
    display.fillRect(65, 0, 66, 20,1);
    display.setCursor(67, 15);
    display.setTextColor(SH110X_BLACK);
    display.print("Water");

    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);

    if(capacity < 10) display.setCursor(84, 47);
    else if (capacity < 100) display.setCursor(72, 47);
    else display.setCursor(60, 47);

    display.print(capacity);

    int tankfill = capacity / 2;
    display.fillRect(2, 62 - tankfill, 57, tankfill, 1);

    display.setTextSize(1);
    display.print("%");

    display.display();
}