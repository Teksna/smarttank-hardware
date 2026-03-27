#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Fonts/FreeSans9pt7b.h>

// ---------------- LoRa ----------------
SX1276 radio = new Module(5, 2, 14, 3);

// ---------------- OLED ----------------
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);

// ---------------- Tank Config ----------------
const int tankHeight = 100; // cm (same as TX assumption)

// ---------------- DISPLAY FUNCTION ----------------
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

  // Volume header
  display.fillRect(65, 0, 66, 20,1);
  display.setCursor(67, 15);
  display.setTextColor(SH110X_BLACK);
  display.print("Volume");

  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);

  if(capacity < 10) display.setCursor(84, 47);
  else if (capacity < 100) display.setCursor(72, 47);
  else display.setCursor(60, 47);

  display.print(capacity);

  // Fill tank
  int tankfill = capacity / 2;
  display.fillRect(2, 62 - tankfill, 57, tankfill, 1);

  display.setTextSize(1);
  display.print("%");

  display.display();
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  // OLED init
  display.begin(0x3C, true);
  display.clearDisplay();
  display.display();

  // LoRa init
  Serial.print("[SX1276] Initializing ... ");

  int state = radio.begin(
    865.0,
    125.0,
    9,
    5,
    0x12,
    17,
    8,
    0
  );

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("success!");
  } else {
    Serial.print("failed, code ");
    Serial.println(state);
    while (true);
  }
}

// ---------------- LOOP ----------------
void loop() {
  Serial.print("Waiting... ");

  String str;

  int state = radio.receive(str);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Received!");

    Serial.print("Raw Data: ");
    Serial.println(str);

    // ---------------- Convert distance → capacity ----------------
    int distance = str.toInt();

    float waterLevel = tankHeight - distance;
    int capacity = (waterLevel / tankHeight) * 100;

    // Clamp
    if (capacity > 100) capacity = 100;
    if (capacity < 0) capacity = 0;

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.print(" cm | Capacity: ");
    Serial.print(capacity);
    Serial.println(" %");

    // ---------------- Display ----------------
    displayStatus(capacity);

    // Debug RF
    Serial.print("RSSI: ");
    Serial.println(radio.getRSSI());

    Serial.print("SNR: ");
    Serial.println(radio.getSNR());

  } 
  else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.println("timeout");
  } 
  else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println("CRC error");
  } 
  else {
    Serial.print("failed, code ");
    Serial.println(state);
  }
}