#include <Arduino.h>
#include <WiFi.h>
#include <RadioLib.h>
#include "display/display.h"
#include "ota/ota.h"

// ---------------- WIFI ----------------
const char* ssid = "Airtel_mohd_3792";
const char* password = "Air@28347";

// ---------------- LORA ----------------
#define ss 5
#define rst 14
#define dio0 2
#define dio1 34

SX1276 radio = new Module(ss, dio0, rst, dio1);

// ---------------- MOTOR ----------------
const int motorPin = 25;
bool motorState = false;

// ---------------- TANK ----------------
const int tankHeight = 100;

// ---------------- STRUCT ----------------
struct LoRaPacket {
    int state;
    String data;
};

// ---------------- RECEIVE FUNCTION ----------------
LoRaPacket receiveLoRa() {
    LoRaPacket pkt;

    pkt.state = radio.receive(pkt.data);

    if (pkt.state == RADIOLIB_ERR_NONE) {
        Serial.println("Received: " + pkt.data);
    }

    return pkt;
}

// ---------------- SETUP ----------------
void setup() {
    Serial.begin(115200);

    // Init display
    initDisplay();

    // Motor setup
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

    // OTA check (once)
    checkForOTAUpdate();
}

// ---------------- LOOP ----------------
void loop() {

    LoRaPacket pkt = receiveLoRa();

    if (pkt.state == RADIOLIB_ERR_NONE) {

        // ---------------- PARSE DATA ----------------
        int distance = pkt.data.toInt();

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

        // ---------------- DISPLAY ----------------
        displayStatus(capacity, radio.getRSSI());

        // ---------------- MOTOR LOGIC ----------------
        if (capacity < 20 && !motorState) {
            digitalWrite(motorPin, HIGH);
            motorState = true;
            Serial.println("Motor ON");
        }
        else if (capacity > 90 && motorState) {
            digitalWrite(motorPin, LOW);
            motorState = false;
            Serial.println("Motor OFF");
        }
    }
    else if (pkt.state == RADIOLIB_ERR_RX_TIMEOUT) {
        Serial.println("timeout");
    }
    else if (pkt.state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println("CRC error");
    }
    else {
        Serial.print("LoRa error: ");
        Serial.println(pkt.state);
    }

    delay(100);
}