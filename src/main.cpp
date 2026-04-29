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
bool motorAutomationState = false;

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


#define FILTER_SIZE 1
#define MAX_VARIATION 20   // 🔥 relaxed (was 10)
#define SENSOR_MAX_PERCENT 75  // 🔥 calibrated max level to avoid false overflow (was 85)

int readings[FILTER_SIZE] = {0};
int readIndex = 0;
bool bufferFilled = false;
bool hasValidReading = false;
int lastStableLevel = 0;
int stableCount = 0;
int getFilteredLevel(int newValue) {

    static int lastValid = -1;
    static bool initialized = false;

    // ---------------- BASIC VALIDATION ----------------
    if (newValue <= 0 || newValue > 100) {
        Serial.println("⚠️ Invalid value → ignored");
        return initialized ? lastValid : 50;
    }

    // ---------------- FIRST VALUE ----------------
    if (!initialized) {
        lastValid = newValue;
        initialized = true;

        readings[readIndex] = newValue;
        readIndex = (readIndex + 1) % FILTER_SIZE;

        return newValue;
    }

    // ---------------- ADD TO BUFFER ----------------
    readings[readIndex] = newValue;
    readIndex = (readIndex + 1) % FILTER_SIZE;

    if (readIndex == 0) bufferFilled = true;

    int sum = 0;
    int count = bufferFilled ? FILTER_SIZE : readIndex;

    for (int i = 0; i < count; i++) {
        sum += readings[i];
    }

    int avg = sum / count;

    // ---------------- SAFETY ----------------
    if (avg <= 0 || avg > 100) {
        return lastValid;
    }

    lastValid = avg;
    return lastValid;
}
void loop() {

    static int overflowThreshold = 90;
    static bool motorDecisionState = false;
    static bool initialized = false;

    static int prevLevel = 50;
    static int stableCount = 0;
    static int noUpdateCount = 0;

    LoRaPacket pkt = receiveLoRa();

    if (pkt.state != RADIOLIB_ERR_NONE) return;

    // ---------------- PARSE ----------------
    int distance = -1;
    float batteryVoltage = 0.0;
    int batteryPercent = 0;

    int firstComma = pkt.data.indexOf(',');
    int secondComma = pkt.data.indexOf(',', firstComma + 1);

    if (firstComma > 0 && secondComma > firstComma) {
        distance = pkt.data.substring(0, firstComma).toInt();
        batteryVoltage = pkt.data.substring(firstComma + 1, secondComma).toFloat();
        batteryPercent = pkt.data.substring(secondComma + 1).toInt();
    } else {
        Serial.println("⚠️ Invalid packet");
        return;
    }

    Serial.print("Distance: ");
    Serial.println(distance);

    // ---------------- VALIDATION ----------------
    if (distance <= 0 || distance > tankHeight) {
        Serial.println("⚠️ Invalid distance → ignored");
        return;
    }

    // ---------------- LEVEL CALC ----------------
    float waterHeight = tankHeight - distance;

    int rawLevel = (waterHeight / tankHeight) * 100;
    rawLevel = constrain(rawLevel, 0, 100);

    int calibrated = (rawLevel * 100) / SENSOR_MAX_PERCENT;
    calibrated = constrain(calibrated, 0, 100);

    int water_level = getFilteredLevel(calibrated);

    if (water_level <= 0) {
        Serial.println("⚠️ Invalid 0% → ignored");
        return;
    }

    // ---------------- INITIALIZE ----------------
    if (!initialized) {
        prevLevel = water_level;
        lastStableLevel = water_level;
        initialized = true;

        Serial.println("✅ Initial level set");
    }

    // ---------------- STABILITY ----------------
    if (abs(water_level - prevLevel) < 30) {
        stableCount++;
    } else {
        stableCount = 0;
    }

    bool isStable = (stableCount >= 1);

    if (isStable) {
        prevLevel = water_level;
        lastStableLevel = water_level;
        noUpdateCount = 0;
    } else {
        noUpdateCount++;

        // 🔥 Anti-freeze fallback
        if (noUpdateCount > 3) {
            Serial.println("⚠️ Force update (anti-freeze)");
            lastStableLevel = water_level;
            prevLevel = water_level;
            noUpdateCount = 0;
        }
    }

    int control_level = lastStableLevel;

    int rssi = radio.getRSSI();

    Serial.print("Water Level: ");
    Serial.print(control_level);
    Serial.print("% | RSSI: ");
    Serial.println(rssi);

    displayStatus(control_level, rssi, batteryPercent);

    // ---------------- CLOUD ----------------
    unsigned long interval = motorState ? 5000 : 10000;

    if (millis() - lastUpload > interval) {

        CloudResponse cloud = ota_device_update_and_fetch(
            control_level,
            motorState,
            rssi,
            batteryPercent
        );

        supplyState = cloud.supply;
        motorAutomationState = cloud.motorAutomation;

        overflowThreshold = (cloud.overflowThreshold > 0)
                            ? cloud.overflowThreshold
                            : 90;

        if (cloud.ota) {
            delay(1000);
            ESP.restart();
        }

        lastUpload = millis();
    }

    // ---------------- HYSTERESIS ----------------
    int lowerBound = max(0, overflowThreshold - 5);
    int upperBound = min(100, overflowThreshold + 3);

    if (motorAutomationState && supplyState) {

        if (control_level < lowerBound) {
            motorDecisionState = true;
        }
        else if (control_level > upperBound) {
            motorDecisionState = false;
        }

    } else {
        motorDecisionState = false;
    }

    bool shouldRunMotor = motorDecisionState;

    // ---------------- WIFI FAILSAFE ----------------
    if (WiFi.status() != WL_CONNECTED) {
        shouldRunMotor = false;
    }

    // ---------------- RELAY ----------------
    if (shouldRunMotor) {

        Serial.println("Motor ON");

        digitalWrite(motorPin_no, LOW);
        digitalWrite(motorPin_nc, LOW);
        delay(200);
        digitalWrite(motorPin_no, HIGH);

        motorState = true;

    } else {

        Serial.println("Motor OFF");

        digitalWrite(motorPin_no, LOW);
        digitalWrite(motorPin_nc, LOW);

        motorState = false;
    }

    delay(100);
}