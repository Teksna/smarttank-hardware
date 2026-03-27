#include <Arduino.h>
#include <WiFi.h>
#include <RadioLib.h>
#include "display/display.h"
#include "ota/ota.h"

// WIFI
const char* ssid = "Airtel_mohd_3792";
const char* password = "Air@28347";
#define ss 5
#define rst 14
#define dio0 2
#define dio1 34

SX1276 radio = new Module(ss, dio0, rst, dio1);

// Motor
const int motorPin = 25;
bool motorState = false;

// IDs
String TXID = "T001";


// TANK
const int tankHeight = 100;

void setup() {
    Serial.begin(115200);

    // Init modules
    initDisplay();
    pinMode(motorPin, OUTPUT);
    digitalWrite(motorPin, LOW);

    // WiFi connect
    WiFi.begin(ssid, password);
    Serial.print("Connecting");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnected!");

    delay(3000);
    Serial.print("Initializing LoRa... ");

    int state = radio.begin(865.0, 125.0, 9, 5, 0x12, 17, 8, 0);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("LoRa OK");
    } else {
        Serial.println("LoRa FAIL");
        while (true);
    }
    // 🔥 OTA check (only once)
    checkForOTAUpdate();
}
void receiveLoRa()
{
    String str;
    int state = radio.receive(str);

    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println("Received: " + str);

        int sep = str.indexOf('|');
        if (sep == -1) return;

        String id = str.substring(0, sep);
        int capacity = str.substring(sep + 1).toInt();

        if (id == TXID)
        {
            displayStatus(capacity);

            // 🔥 MOTOR LOGIC
            if (capacity < 20 && !motorState)
            {
                digitalWrite(motorPin, HIGH);
                motorState = true;
                Serial.println("Motor ON");
            }
            else if (capacity > 90 && motorState)
            {
                digitalWrite(motorPin, LOW);
                motorState = false;
                Serial.println("Motor OFF");
            }
        }
    }
}

void loop() {
    receiveLoRa();
    delay(100);
}