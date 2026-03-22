#include <Arduino.h>
#include <WiFi.h>

#include "display/display.h"
#include "sensor/ultrasonic.h"
#include "ota/ota.h"

// WIFI
const char* ssid = "Airtel_mohd_3792";
const char* password = "Air@28347";

// PINS
const int trigPin = 26;
const int echoPin = 27;

// TANK
const int tankHeight = 100;

void setup() {
    Serial.begin(115200);

    // Init modules
    initDisplay();
    initUltrasonic(trigPin, echoPin);

    // WiFi connect
    WiFi.begin(ssid, password);
    Serial.print("Connecting");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnected!");

    delay(3000);

    // 🔥 OTA check (only once)
    checkForOTAUpdate();
}

void loop() {
    int capacity = getTankCapacity(tankHeight);

    Serial.print("Capacity: ");
    Serial.print(capacity);
    Serial.println("%");

    displayStatus(capacity);

    delay(500);
}