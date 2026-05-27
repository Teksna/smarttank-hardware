#include "display.h"

#include <SPI.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

// ---------------- TFT PINS ----------------
#define TFT_CS     33
#define TFT_RST    25
#define TFT_DC     32

// ---------------- COLORS ----------------
#define BG_COLOR       ST77XX_BLACK
#define HEADER_COLOR   ST77XX_BLUE
#define WATER_COLOR    ST77XX_CYAN
#define BORDER_COLOR   ST77XX_WHITE
#define TEXT_COLOR     ST77XX_WHITE

#define MOTOR_ON_COLOR     ST77XX_GREEN
#define MOTOR_OFF_COLOR    ST77XX_RED

// ---------------- TFT OBJECT ----------------
Adafruit_ST7735  display =
    Adafruit_ST7735 (
        TFT_CS,
        TFT_DC,
        TFT_RST
    );
void drawBatteryIcon(
    int x,
    int y,
    int percent
)
{
    // Smaller outer body
    display.drawRect(x, y, 20, 10, ST77XX_WHITE);

    // Smaller battery tip
    display.fillRect(x + 20, y + 3, 2, 4, ST77XX_WHITE);

    // Fill level
    int fillWidth = map(percent, 0, 100, 0, 16);

    uint16_t color;

    if(percent > 60)
        color = ST77XX_GREEN;
    else if(percent > 25)
        color = ST77XX_YELLOW;
    else
        color = ST77XX_RED;

    // Battery fill
    display.fillRect(
        x + 2,
        y + 2,
        fillWidth,
        6,
        color
    );
}
void drawSignalIcon(
    int x,
    int y,
    int rssi
)
{
    uint16_t color;

    // Signal quality color
    if(rssi > -70)
        color = ST77XX_GREEN;
    else if(rssi > -85)
        color = ST77XX_YELLOW;
    else
        color = ST77XX_RED;

    // Small center dot
    display.fillCircle(x, y, 2, color);

    // Arc 1
    display.drawCircle(x, y, 6, color);

    // Arc 2
    display.drawCircle(x, y, 10, color);

    // Arc 3
    display.drawCircle(x, y, 14, color);

    // Hide lower halves to create WiFi shape
    display.fillRect(x - 16, y, 32, 20, ST77XX_BLACK);
    
    display.fillRect(x , y-16, 18, 18, ST77XX_BLACK);
}
void drawMotorIcon(
    int x,
    int y,
    bool running
)
{
    uint16_t bodyColor;

    if(running)
        bodyColor = ST77XX_CYAN;
    else
        bodyColor = ST77XX_BLUE;

    // =====================================================
    // CONTROL BOX
    // =====================================================

    display.fillRect(
        x,
        y,
        16,
        12,
        bodyColor
    );

    display.fillRoundRect(
        x + 2,
        y + 2,
        9,
        6,
        2,
        ST77XX_BLACK
    );

    display.fillRect(
        x + 5,
        y + 4,
        4,
        2,
        ST77XX_WHITE
    );

    display.fillCircle(
        x + 14,
        y + 4,
        1,
        ST77XX_BLACK
    );

    display.fillCircle(
        x + 14,
        y + 8,
        1,
        ST77XX_BLACK
    );

    // Pipe

    display.fillRect(
        x + 6,
        y + 13,
        3,
        4,
        0x8410
    );

    // =====================================================
    // LEFT BODY
    // =====================================================

    display.fillRect(
        x + 3,
        y + 22,
        13,
        26,
        bodyColor
    );

    // Side shade

    display.fillRect(
        x + 3,
        y + 22,
        2,
        26,
        ST77XX_BLUE
    );

    // Bolts

    display.fillCircle(
        x + 6,
        y + 27,
        1,
        ST77XX_BLACK
    );

    display.fillCircle(
        x + 13,
        y + 27,
        1,
        ST77XX_BLACK
    );

    display.fillCircle(
        x + 6,
        y + 43,
        1,
        ST77XX_BLACK
    );

    display.fillCircle(
        x + 13,
        y + 43,
        1,
        ST77XX_BLACK
    );

    // =====================================================
    // SHAFT
    // =====================================================

    display.fillRect(
        x + 18,
        y + 22,
        2,
        26,
        ST77XX_WHITE
    );

    display.fillRect(
        x + 22,
        y + 25,
        2,
        20,
        0x8410
    );

    // =====================================================
    // TURBINE BODY
    // =====================================================

    display.fillRect(
        x + 28,
        y + 22,
        26,
        26,
        bodyColor
    );

    // Rounded end

    display.fillRoundRect(
        x + 51,
        y + 22,
        8,
        26,
        3,
        ST77XX_WHITE
    );

    // =====================================================
    // TURBINE LINES
    // =====================================================

    for(int i = 0; i < 4; i++)
    {
        display.drawFastHLine(
            x + 32,
            y + 27 + (i * 5),
            16,
            ST77XX_BLACK
        );
    }

    // =====================================================
    // BASE
    // =====================================================

    display.fillRect(
        x + 3,
        y + 54,
        50,
        2,
        0x8410
    );

    // Feet

    display.fillRect(
        x + 8,
        y + 50,
        6,
        2,
        ST77XX_WHITE
    );

    display.fillRect(
        x + 38,
        y + 50,
        6,
        2,
        ST77XX_WHITE
    );
}
    // ---------------- ANIMATION ----------------
int waveOffset = 0;

// ======================================================
// WATER ANIMATION
// ======================================================


// ======================================================
// INIT DISPLAY
// ======================================================

void initDisplay()
{
    Serial.println("Initializing display...");
    display.initR(INITR_BLACKTAB);

    // Landscape
    display.setRotation(1);

    display.fillScreen(BG_COLOR);

    display.setTextWrap(false);
    display.setFont(&FreeSans9pt7b);
}

// ======================================================
// MAIN STATUS SCREEN
// ======================================================

void displayStatus(
    
    int capacity,
    int rssi,
    int batteryPercent
)
{
   // =====================================================
    // CLEAR SCREEN
    // =====================================================

    display.fillScreen(ST77XX_BLACK);

    // =====================================================
    // TANK DIMENSIONS
    // =====================================================

    int tankLeft   = 0;
    int tankRight  = 78;

    int tankTop    = 24;
    int tankBottom = 127;

    // =====================================================
    // TANK WALLS
    // =====================================================

    display.drawLine(
        tankLeft,
        tankBottom,
        tankLeft,
        tankTop,
        ST77XX_WHITE
    );

    display.drawLine(
        tankRight,
        tankBottom,
        tankRight,
        tankTop,
        ST77XX_WHITE
    );

    display.drawLine(
        tankLeft,
        tankBottom,
        tankRight,
        tankBottom,
        ST77XX_WHITE
    );
    // Top horizontal wall
    display.drawLine(
        tankLeft,
        tankTop,
        tankRight,
        tankTop,
        ST77XX_WHITE
    );
    // =====================================================
// INNER TANK WALLS
// =====================================================



    // =====================================================
    // TANK TOP /\ SHAPE
    // =====================================================

    display.drawLine(
        tankLeft,
        tankTop,
        24,
        4,
        ST77XX_WHITE
    );

    display.drawLine(
        54,
        4,
        tankRight,
        tankTop,
        ST77XX_WHITE
    );

    // Tank lid
    display.fillRect(
        24,
        0,
        30,
        8,
        ST77XX_WHITE
    );

    // Under lid line
    display.drawLine(
        24,
        4,
        54,
        4,
        ST77XX_WHITE
    );

    

    // =====================================================
    // WATER LEVEL
    // =====================================================

    int tankFill = map(
        capacity,
        0,
        100,
        0,
        97
    );

    // Blue water
    display.fillRect(
        2,
        124 - tankFill,
        75,
        tankFill,
        ST77XX_BLUE
    );

    // =====================================================
    // RIGHT PANEL
    // =====================================================

    int panelX = 84;

    // =====================================================
    // HEADER
    // =====================================================

    display.fillRect(
        panelX,
        0,
        76,
        24,
        ST77XX_WHITE
    );

    display.setTextColor(ST77XX_BLACK);

    

    
    display.setFont(&FreeSans12pt7b);
    display.setTextSize(1);
    display.setCursor(
        panelX + 7,
        20

    );
    display.print("Water");

    // =====================================================
    // BIG VALUE
    // =====================================================

    display.setTextColor(ST77XX_WHITE);
    display.setFont(&FreeSans24pt7b);
    display.setTextSize(1);

    if(capacity < 10) {

        display.setCursor(panelX + 24, 68);
        display.print(capacity);
        display.setFont();
        // display.setCursor(panelX + 47, 29);  
        display.setTextSize(1);
        display.print("%");

    } else if(capacity < 100) {

        display.setCursor(panelX + 8, 68);
        display.print(capacity);
        display.setFont();
        // display.setCursor(panelX + 57, 29);  
        display.setTextSize(1);
        display.print("%");

    } else {

        display.setCursor(panelX-8, 68);
        display.print(capacity);
        display.setFont();
        // display.setCursor(panelX + 67, 29);  
        display.setTextSize(1);
        display.print("%");
    } 

    

    // =====================================================
    // DIVIDER
    // =====================================================

    display.drawLine(
        panelX,
        83,
        159,
        83,
        ST77XX_WHITE
    );

    // =====================================================
    // RSSI
    // =====================================================

    display.setTextColor(ST77XX_GREEN);
    display.setTextSize(1);

    display.setCursor(panelX + 2, 98);
    display.setFont(&FreeSans9pt7b);
    drawSignalIcon(103, 103, -70);

    display.setTextColor(ST77XX_WHITE);
    display.setCursor(panelX + 34, 102);
    display.print(rssi);

    // =====================================================
    // BATTERY
    // =====================================================

    display.setTextColor(ST77XX_YELLOW);
    display.setCursor(panelX - 20, 148);
    drawBatteryIcon(panelX+2, 112, 82);
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(panelX + 32, 122);
    display.print(batteryPercent);

    // =====================================================
    // ANIMATION
    // =====================================================

    capacity += 10;
    if(capacity > 100) {

        capacity = 0;
    }
    delay(2000);
    Serial.println("Cleaning up display...");
    display.fillScreen(ST77XX_BLACK);
    
    // if (status)
    //     status = false;
    // else
    //     status = true;
    // motorStatus(status);
    // delay(2000);
    // display.fillScreen(ST77XX_BLACK);

}


void displayMotorStatus(bool motorRunning)
{
    bool status = motorRunning;
    display.fillRect(
        0,
        0,
        160,
        30,
        ST77XX_WHITE
    );

    display.setTextColor(ST77XX_BLACK);

    display.setFont(&FreeSans12pt7b);

    display.setCursor(28, 22);

    display.print("MOTOR");

    uint16_t motorColor;

    if(status)
        motorColor = ST77XX_GREEN;
    else
        motorColor = ST77XX_RED;

    // Draw motor
    drawMotorIcon(
        15,
        48,
        motorColor
    );

    // ON/OFF text
    display.setFont(&FreeSans12pt7b);

    display.setTextColor(ST77XX_WHITE);

   // =====================================================
// STATUS CIRCLE
// =====================================================

uint16_t statusColor;

if(status)
    statusColor = ST77XX_GREEN;
else
    statusColor = ST77XX_ORANGE;

// Circle beside motor

display.fillCircle(
    118,     // X
    78,     // Y
    33,     // Radius
    statusColor
);

// =====================================================
// TEXT INSIDE CIRCLE
// =====================================================

display.setTextColor(ST77XX_BLACK);

if(status)
{
    display.setCursor(
        98,
        86
    );

    display.print("ON");
}
else
{
    display.setCursor(
        93,
        86
    );

    display.print("OFF");
}
}
// ======================================================
// CLEANUP
// ======================================================

void cleanupDisplay()
{
    Serial.println("Cleaning up display...");
    display.fillScreen(ST77XX_BLACK);
}