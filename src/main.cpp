#include <Arduino.h>
#include <WiFi.h>
#include <RadioLib.h>

#include "display/display.h"
#include "ota/ota.h"
#include "supabase/supabase.h"
#include "mywifi.cpp"

// ---------------- WIFI ----------------
// const char* ssid = "Airtel_mohd_3792";
// const char* password = "Air@28347"; 
const char* ssid1 = "Airtel_mohd_3792";
const char* pass1 = "Air@283477";

const char* ssid2 = "airtel_umai_0972";
const char* pass2 = "Your_Other_Password";

void connectWiFi() {
    Serial.println("Connecting to WiFi 1...");
    WiFi.begin(ssid1, pass1);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi 1 ✅");
        return;
    }

    // Try second WiFi
    Serial.println("\nWiFi 1 failed. Trying WiFi 2...");
    WiFi.begin(ssid2, pass2);

    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected to WiFi 2 ✅");
    } else {
        Serial.println("\nWiFi Failed ❌");
    }
}


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
    // WiFi.begin(ssid, password);
    Serial.print("Connecting");
     WiFi.mode(WIFI_STA);

    connectWiFi();
    // while (WiFi.status() != WL_CONNECTED) {
    //     delay(500);
    //     Serial.print(".");
       
    // }

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



#define SENSOR_MAX_PERCENT 80  // 🔥 calibrated max level to avoid false overflow (was 85)

int FULL = 19;     // tank full
int EMPTY = 120;   // tank empty (adjust)
int lastStableLevel = 0;
int stableCount = 0;
int calculateWaterLevel(int distance) {

  

  if (distance <= FULL) return 100;
  if (distance >= EMPTY) return 0;

  float percent = (float)(EMPTY - distance) * 100.0 / (EMPTY - FULL);

  return (int)(percent + 0.5);
}
static bool motorDecisionState = false;
void loop() {
    Serial.println("Listening for LoRa packets...");

    static int overflowThreshold = 90;
   
    static bool initialized = false;

    static int prevLevel = 50;
    static int stableCount = 0;
    static int noUpdateCount = 0;

    LoRaPacket pkt = receiveLoRa();

    if (pkt.state != RADIOLIB_ERR_NONE) return;
    Serial.println("Processing packet...");
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


     int water_level = calculateWaterLevel(distance);


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

    // displayStatus(control_level, rssi, batteryPercent);
    // delay(300);

    // ---------------- CLOUD ----------------
    unsigned long interval = motorState ? 3000 : 3000;

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
        
        EMPTY = overflowThreshold; 

        if (cloud.ota) {
            delay(1000);
            ESP.restart();
        }

        lastUpload = millis();
    }

    // ---------------- HYSTERESIS ----------------
    int lowerBound = max(0, overflowThreshold );
    int upperBound = min(100, overflowThreshold);

    if (motorAutomationState && supplyState) {
        Serial.println("Motor automation enabled and supply available");

        if (control_level < 70 && !motorDecisionState) {
            Serial.println("Motor ON condition met");
            motorDecisionState = true;
        }
        else if (control_level > upperBound) {
            Serial.println("Motor OFF condition met");

            motorDecisionState = false;
        }

    } else {
        motorDecisionState = false;
        Serial.println("Motor automation disabled or no supply");
    }

    bool shouldRunMotor = motorDecisionState;

    // ---------------- WIFI FAILSAFE ----------------
    if (WiFi.status() != WL_CONNECTED) {
        shouldRunMotor = false;
    }

    // ---------------- RELAY ----------------
    if (shouldRunMotor ) {

        Serial.println("Motor ON");

        digitalWrite(motorPin_no, HIGH);
        motorState = true;
        displayMotorStatus(motorState);
        delay(2000);
        displayStatus(control_level, rssi, batteryPercent);
        delay(2000);
            

    } else {

        delay(200);
        Serial.println("Motor OFF");
        digitalWrite(motorPin_no, LOW);
        motorState = false;
        displayMotorStatus(motorState);
        delay(2000);
        displayStatus(control_level, rssi, batteryPercent);
        delay(2000);
    }

    delay(100);
}