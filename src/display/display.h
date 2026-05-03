#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_SH110X.h>

void initDisplay();
void displayStatus(int capacity, int rssi, int batteryPercent);
void displayMotorStatus(bool motorRunning);

#endif