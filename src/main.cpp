#include <Arduino.h>
#include <WiFi.h>
#include <RadioLib.h>
#include "display/display.h"
#include "ota/ota.h"
#include <HTTPClient.h>
const char* supabaseUrl = "https://yljggigahlagdihhycfj.supabase.co";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlsamdnaWdhaGxhZ2RpaGh5Y2ZqIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzI3MDcwNTMsImV4cCI6MjA4ODI4MzA1M30.NeGRlQv-T-OGW4iqJPLV2T-2uPQxDNz0r9GHLAG8F-g";
bool cloudSupply = false;
bool fetchSupplyState() {

    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;

    String url = String(supabaseUrl) + 
      "/rest/v1/iot_logs?select=supply&order=created_at.desc&limit=1";

    http.begin(url);
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", "Bearer " + String(supabaseKey));

    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();

        // Simple parse (lightweight)
        if (payload.indexOf("true") > 0) {
            cloudSupply = true;
        } else {
            cloudSupply = false;
        }

        Serial.print("Cloud Supply: ");
        Serial.println(cloudSupply);
    } else {
        Serial.print("Fetch Error: ");
        Serial.println(httpCode);
    }

    http.end();
    return cloudSupply;
}
void sendToSupabase(int tank, bool motor, int rssi) {

    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;

    String url = String(supabaseUrl) + "/rest/v1/iot_logs";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", supabaseKey);
    http.addHeader("Authorization", "Bearer " + String(supabaseKey));
    http.addHeader("Prefer", "return=minimal"); // ⚡ faster

    // JSON payload (lightweight)
    String payload = "{";
    payload += "\"username\":\"tank1\",";
    payload += "\"motor\":" + String(motor ? "true" : "false") + ",";
    payload += "\"tank\":" + String(tank) + ",";
    payload += "\"rssi\":" + String(rssi);
    payload += "}";

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
        Serial.print("Supabase OK: ");
        Serial.println(httpResponseCode);
    } else {
        Serial.print("Supabase Error: ");
        Serial.println(httpResponseCode);
    }

    http.end(); // free memory
}
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
    unsigned long lastFetch = 0;
    bool supplyState;
    // 🔄 Fetch supply every 3 sec
    if (millis() - lastFetch > 3000) {
        supplyState = fetchSupplyState();
        lastFetch = millis();
    }
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
        int rssi = radio.getRSSI();
        Serial.print("RSSI: ");
        Serial.print(rssi);
        
        // ---------------- DISPLAY ----------------
        displayStatus(capacity, rssi);

        // ---------------- MOTOR LOGIC ----------------
        if (capacity < 20 && !motorState && supplyState) {
            digitalWrite(motorPin, HIGH);
            motorState = true;
            Serial.println("Motor ON");
        }
        else if (capacity > 90 && motorState && !supplyState) {
            digitalWrite(motorPin, LOW);
            motorState = false;
            Serial.println("Motor OFF");
        }
        unsigned long lastUpload = 0;
        if (millis() - lastUpload > 5000) {  // every 5 sec
        sendToSupabase(capacity, motorState, rssi);
        lastUpload = millis();
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