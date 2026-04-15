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
const int motorPin_no = 27; // GPIO pin to control the motor (normally open relay)
const int motorPin_nc = 26; // GPIO pin to control the motor (normally closed relay)
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

    pinMode(motorPin_no, OUTPUT);
    pinMode(motorPin_nc, OUTPUT);
    digitalWrite(motorPin_nc, LOW); // Ensure normally closed relay is active to keep motor off
    delay(2000); // brief delay to ensure relay state change
    digitalWrite(motorPin_no, LOW); // Ensure normally open relay is inactive to keep motor off
    Serial.println("Motor Pins Initialized");
    delay(1000);

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

    LoRaPacket pkt = receiveLoRa();

    if (pkt.state == RADIOLIB_ERR_NONE) {

        int distance = -1;
        float batteryVoltage = 0.0;
        int batteryPercent = 0;

        // Parse CSV
        int firstComma = pkt.data.indexOf(',');
        int secondComma = pkt.data.indexOf(',', firstComma + 1);

        if (firstComma > 0 && secondComma > firstComma) {
            distance = pkt.data.substring(0, firstComma).toInt();
            batteryVoltage = pkt.data.substring(firstComma + 1, secondComma).toFloat();
            batteryPercent = pkt.data.substring(secondComma + 1).toInt();
        }

        // Debug
        Serial.print("Distance: ");
        Serial.println(distance);

        Serial.print("Battery Voltage: ");
        Serial.println(batteryVoltage);

        Serial.print("Battery %: ");
        Serial.println(batteryPercent);

        float waterLevel = tankHeight - distance;
        int capacity = (waterLevel / tankHeight) * 100;

        if (capacity > 100) capacity = 100;
        if (capacity < 0) capacity = 0;

        int rssi = radio.getRSSI();

        Serial.print("Capacity: ");
        Serial.print(capacity);
        Serial.print(" % | RSSI: ");
        Serial.println(rssi);

        displayStatus(capacity, rssi, batteryPercent);

        // ---------------- MOTOR LOGIC ----------------
        if (!supplyState) {
            Serial.println("Supply OFF - Motor OFF");
            digitalWrite(motorPin_no, LOW); // Ensure normally open relay is inactive to keep motor off
            delay(1000); // brief delay to ensure relay state change
            digitalWrite(motorPin_nc, LOW); // Ensure normally closed relay is active to keep motor off
            delay(1000); // brief delay to ensure relay state change
            motorState = false;
        } else {
            if (capacity < 90) {
                Serial.println("Supply ON - Motor ON");
                digitalWrite(motorPin_nc, LOW); // Activate normally closed relay
                delay(1000); // brief delay to ensure relay state change
                digitalWrite(motorPin_no, HIGH); // Activate normally open relay
                            
                motorState = true;
            } else {
                Serial.println("Tank Full - Motor OFF");
                digitalWrite(motorPin_no, LOW); // Deactivate normally open relay
                delay(1000); // brief delay to ensure relay state change
                digitalWrite(motorPin_nc, LOW); // Keep normally closed relay active to ensure motor is off
                motorState = false;
            }
        }

        // ---------------- CLOUD SYNC ----------------
        unsigned long interval;

        if (motorState) {
            interval = 5000;  // 5 sec when motor ON
        } else {
            interval = 10000;  // 10 sec when motor OFF
        }

        if (millis() - lastUpload > interval) {
            Serial.println("Syncing with Supabase (RPC)...");

            CloudResponse cloud = OTAupdateAndFetchSupply(capacity, motorState, rssi, batteryPercent);

            supplyState = cloud.supply;

            if (cloud.ota) {
                Serial.println("OTA Trigger received → Rebooting...");

                delay(1000);
                ESP.restart();   // 🔥 remote reboot
            }

            Serial.print("Cloud Supply: ");
            Serial.println(supplyState);

            lastUpload = millis();
        }
    }

    delay(100);
}