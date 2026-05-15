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

void displayStatus(int capacity, int rssi, int batteryPercent)
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

    
    display.display();

    // Header
    display.fillRect(65, 0, 66, 17,1);
    display.setCursor(73, 15);
    display.setTextColor(SH110X_BLACK);
    display.print("Water");

    // ---------------- CAPACITY ----------------
    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);

    // Adjust alignment
    if(capacity < 10) {
        display.setCursor(84, 44);
    } else if (capacity < 100) {
        display.setCursor(72, 44);
    } else {
        display.setCursor(60, 44);
    }

    display.print(capacity);

    int tankfill = capacity/2;
  display.fillRect(2,62-tankfill , 57, tankfill,1);
  //display.fillRect(2,12 , 56, 50,1);
  display.setFont();
  display.setTextSize(1);
  display.println("%");
  display.display();
  ///////
  display.setCursor(90,40);
  //display.print(str);
  display.setTextSize(1);
  display.setFont();
  /* if (state == 0)
  {
  display.setCursor(0,54);
  display.print("Connected");
  }
  else
  { 
  display.setCursor(0,54);
  display.print("Not Connected");
  }*/
  // ---------------- RSSI & BATTERY ----------------
  display.drawLine(65, 50, display.width(), 50, SH110X_WHITE);

  display.fillRect(65, 52, 8, 20,1);
  display.setCursor(66,54);
  display.setTextColor(SH110X_BLACK);   
  display.println("S:");
  display.setTextColor(SH110X_WHITE);
  display.setCursor(74,54);
  display.println(rssi);
  display.display();

  display.fillRect(99, 55, 7, 20,1);
  display.fillRect(100, 52, 5, 2,1);
  display.setCursor(108,54);
  display.print(batteryPercent);
  display.setFont();
  display.setTextSize(1);
  display.print("%");
  display.display();
}

void displayMotorStatus(bool motorRunning)
{
    display.clearDisplay();

    display.setFont(&FreeSans9pt7b);

    // ---------------- MOTOR HEADER (BLACK BG + WHITE TEXT) ----------------
    display.fillRect(0, 0, 128, 22, SH110X_WHITE);   // white rectangle
    display.setTextColor(SH110X_BLACK);              // black text
    display.setCursor(28, 16);
    display.setTextSize(1);
    display.print("MOTOR");

    // ---------------- ON / OFF BIG TEXT ----------------
    display.setTextColor(SH110X_WHITE);              // back to white text
    display.setTextSize(2);

    if (motorRunning) {
        display.setCursor(34, 50);
        display.print("ON");
    } else {
        display.setCursor(22, 50);
        display.print("OFF");
    }

    display.display();
}
void cleanupDisplay() {
    display.clearDisplay();
    display.display();
}