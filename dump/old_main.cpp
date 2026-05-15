// #include <Arduino.h>
// #include <WiFi.h>
// #include <RadioLib.h>

// #include "display/display.h"
// #include "ota/ota.h"
// #include "supabase/supabase.h"

// // ---------------- WIFI ----------------
// const char* ssid = "Airtel_mohd_3792";
// const char* password = "Air@28347";

// // ---------------- SUPABASE ----------------
// const char* supabaseUrl = "https://yljggigahlagdihhycfj.supabase.co";
// const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlsamdnaWdhaGxhZ2RpaGh5Y2ZqIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzI3MDcwNTMsImV4cCI6MjA4ODI4MzA1M30.NeGRlQv-T-OGW4iqJPLV2T-2uPQxDNz0r9GHLAG8F-g";

// // ---------------- LORA ----------------
// #define ss 5
// #define rst 14
// #define dio0 2
// #define dio1 34

// SX1276 radio = new Module(ss, dio0, rst, dio1);

// // ---------------- MOTOR ----------------
// const int motorPin_no = 27; // GPIO pin to control the motor (normally open relay)
// const int motorPin_nc = 26; // GPIO pin to control the motor (normally closed relay)
// bool motorState = false;

// // ---------------- TANK ----------------
// const int tankHeight = 100;

// // ---------------- TIMERS ----------------
// unsigned long lastFetch = 0;
// unsigned long lastUpload = 0;
// bool supplyState = false;
// bool motorAutomationState = false;

// // ---------------- STRUCT ----------------
// struct LoRaPacket {
//     int state;
//     String data;
// };

// // ---------------- RECEIVE FUNCTION ----------------
// LoRaPacket receiveLoRa() {
//     LoRaPacket pkt;
//     pkt.state = radio.receive(pkt.data);

//     if (pkt.state == RADIOLIB_ERR_NONE) {
//         Serial.println("Received: " + pkt.data);
//     }

//     return pkt;
// }

// // ---------------- SETUP ----------------
// void setup() {
//     Serial.begin(115200);

//     initDisplay();

//     pinMode(motorPin_no, OUTPUT);
//     pinMode(motorPin_nc, OUTPUT);
//     digitalWrite(motorPin_nc, LOW); // Ensure normally closed relay is active to keep motor off
//     delay(2000); // brief delay to ensure relay state change
//     digitalWrite(motorPin_no, LOW); // Ensure normally open relay is inactive to keep motor off
//     Serial.println("Motor Pins Initialized");
//     delay(1000);

//     // WiFi
//     WiFi.begin(ssid, password);
//     Serial.print("Connecting");

//     while (WiFi.status() != WL_CONNECTED) {
//         delay(500);
//         Serial.print(".");
//     }

//     Serial.println("\nConnected!");

//     // Supabase init
//     supabaseInit(supabaseUrl, supabaseKey);

//     // LoRa init
//     Serial.print("[SX1276] Initializing ... ");

//     int state = radio.begin(
//         865.0,
//         125.0,
//         9,
//         5,
//         0x12,
//         17,
//         8,
//         0
//     );

//     if (state == RADIOLIB_ERR_NONE) {
//         Serial.println("success!");
//     } else {
//         Serial.print("failed, code ");
//         Serial.println(state);
//         while (true);
//     }

//     // OTA
//     checkForOTAUpdate();
// }

// // ---------------- LOOP ----------------
// void loop() {

//     static int overflowThreshold = 90; // 🔥 persistent threshold

//     LoRaPacket pkt = receiveLoRa();

//     if (pkt.state == RADIOLIB_ERR_NONE) {

//         int distance = -1;
//         float batteryVoltage = 0.0;
//         int batteryPercent = 0;

//         // ---------------- PARSE DATA ----------------
//         int firstComma = pkt.data.indexOf(',');
//         int secondComma = pkt.data.indexOf(',', firstComma + 1);

//         if (firstComma > 0 && secondComma > firstComma) {
//             distance = pkt.data.substring(0, firstComma).toInt();
//             batteryVoltage = pkt.data.substring(firstComma + 1, secondComma).toFloat();
//             batteryPercent = pkt.data.substring(secondComma + 1).toInt();
//         }

//         // ---------------- DEBUG ----------------
//         Serial.print("Distance: "); Serial.println(distance);
//         Serial.print("Battery Voltage: "); Serial.println(batteryVoltage);
//         Serial.print("Battery %: "); Serial.println(batteryPercent);

//         // ---------------- CALCULATE LEVEL ----------------
//         float waterLevel = tankHeight - distance;
//         int water_level = (waterLevel / tankHeight) * 100;

//         if (water_level > 100) water_level = 100;
//         if (water_level < 0) water_level = 0;

//         int rssi = radio.getRSSI();

//         Serial.print("Water Level: ");
//         Serial.print(water_level);
//         Serial.print(" % | RSSI: ");
//         Serial.println(rssi);

//         displayStatus(water_level, rssi, batteryPercent);

//         // ---------------- CLOUD SYNC ----------------
//         unsigned long interval = motorState ? 7000 : 14000;

//         if (millis() - lastUpload > interval) {

//             Serial.println("Syncing with Supabase (RPC)...");

//             CloudResponse cloud = ota_device_update_and_fetch(
//                 water_level,
//                 motorState,
//                 rssi,
//                 batteryPercent
//             );

//             // 🔥 Update cloud-driven states FIRST
//             supplyState = cloud.supply;
//             motorAutomationState = cloud.motorAutomation;

//             // 🔥 Update threshold safely
//             overflowThreshold = (cloud.overflowThreshold > 0)
//                                 ? cloud.overflowThreshold
//                                 : 90;

//             if (cloud.ota) {
//                 Serial.println("OTA Trigger received → Rebooting...");
//                 delay(1000);
//                 ESP.restart();
//             }

//             Serial.print("Cloud Supply: ");
//             Serial.println(supplyState);

//             Serial.print("Automation: ");
//             Serial.println(motorAutomationState);

//             Serial.print("Overflow Threshold: ");
//             Serial.println(overflowThreshold);

//             lastUpload = millis();
//         }

//         // ---------------- MOTOR DECISION ----------------
//         bool shouldRunMotor =
//             motorAutomationState &&
//             supplyState &&
//             (water_level < overflowThreshold);

//         // ---------------- WIFI FAILSAFE ----------------
//         if (WiFi.status() != WL_CONNECTED) {
//             Serial.println("WiFi LOST → Motor OFF (FAILSAFE)");
//             shouldRunMotor = false;
//         }

//         // ---------------- APPLY RELAY SAFELY ----------------
//         if (shouldRunMotor) {

//             Serial.println("AUTO ON → Motor ON");

//             // Turn everything OFF first
//             digitalWrite(motorPin_no, LOW);
//             digitalWrite(motorPin_nc, LOW);
//             delay(200);

//             // Then turn ON motor
//             digitalWrite(motorPin_no, HIGH);

//             motorState = true;

//         } else {

//             Serial.println("Motor OFF");

//             digitalWrite(motorPin_no, LOW);
//             digitalWrite(motorPin_nc, LOW);

//             motorState = false;
//         }
//     }

//     delay(100);
// }