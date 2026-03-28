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

void displayStatus(int capacity, int rssi)
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

    display.drawLine(85, 52, display.width(), 52, SH110X_WHITE);
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
        display.setCursor(84, 46);
    } else if (capacity < 100) {
        display.setCursor(72, 46);
    } else {
        display.setCursor(60, 46);
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
  
  display.fillRect(65, 52, 64, 20,1);
  display.setCursor(68,54);
  display.setTextColor(SH110X_BLACK);   
  display.println("RSSI: ");
  
//   display.drawLine(102, 52, 102, display.height(), SH110X_WHITE);
//   display.drawLine(112, 52, display.width(), 52, SH110X_WHITE);
  
  display.setTextColor(SH110X_BLACK);
  display.setCursor(104,54);
  //display.print("\nRSSI ");
  display.println(rssi);
  display.display();
}