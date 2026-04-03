#include <Arduino.h>
#include <WiFi.h>
#include <RadioLib.h>

#include "display/display.h"
#include "ota/ota.h"
#include "supabase/supabase.h"

// ---------------- WIFI ----------------
const char* ssid = "Airtel_mohd_3792";
const char* password = "Air@28347";

// ---------------- SUPABASE ----------------
const char* supabaseUrl = "https://yljggigahlagdihhycfj.supabase.co";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlsamdnaWdhaGxhZ2RpaGh5Y2ZqIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzI3MDcwNTMsImV4cCI6MjA4ODI4MzA1M30.NeGRlQv-T-OGW4iqJPLV2T-2uPQxDNz0r9GHLAG8F-g";

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

// ---------------- TIMERS ----------------
unsigned long lastFetch = 0;
unsigned long lastUpload = 0;
bool supplyState = false;

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

    initDisplay();

    pinMode(motorPin, OUTPUT);
    digitalWrite(motorPin, LOW);

    // WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnected!");

    // Supabase init
    supabaseInit(supabaseUrl, supabaseKey);

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

    // OTA
    checkForOTAUpdate();
}

// ---------------- LOOP ----------------
void loop() {

    //  Fetch supply from cloud every 3 sec
    if (millis() - lastFetch > 3000) {
        supplyState = fetchSupplyState();
        lastFetch = millis();

        Serial.print("Cloud Supply: ");
        Serial.println(supplyState);
    }

    LoRaPacket pkt = receiveLoRa();

    if (pkt.state == RADIOLIB_ERR_NONE) {

        int distance = pkt.data.toInt();

        float waterLevel = tankHeight - distance;
        int capacity = (waterLevel / tankHeight) * 100;

        if (capacity > 100) capacity = 100;
        if (capacity < 0) capacity = 0;

        int rssi = radio.getRSSI();

        Serial.print("Capacity: ");
        Serial.print(capacity);
        Serial.print(" % | RSSI: ");
        Serial.println(rssi);

        displayStatus(capacity, rssi);

        // ---------------- MOTOR LOGIC ----------------
        // ---------------- MOTOR LOGIC ----------------
        if (!supplyState) {
            // ❌ Supply OFF → always OFF
            Serial.println("Supply OFF - Motor OFF");
            digitalWrite(motorPin, LOW);
            motorState = false;
        }
        else {
            // ✅ Supply ON → fill tank
            if (capacity < 90) {
                Serial.println("Supply ON - Motor ON");
                digitalWrite(motorPin, HIGH);
                motorState = true;
            } else {
                Serial.println("Tank Full - Motor OFF");
                digitalWrite(motorPin, LOW);
                motorState = false;
            }
        }

        // ---------------- SEND DATA ----------------
        if (millis() - lastUpload > 5000) {
            Serial.println("Uploading data to Supabase...");
            updateDeviceState(capacity, motorState, rssi);
            lastUpload = millis();
        }
    }

    delay(100);
}