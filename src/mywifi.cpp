// #include <Arduino.h>
// // ---------------- WIFI ----------------
// // const char* ssid = "Airtel_mohd_3792";
// // const char* password = "Air@28347"; 
// const char* ssid1 = "Airtel_mohd_3792";
// const char* pass1 = "Air@28347";

// const char* ssid2 = "Your_Other_WiFi";
// const char* pass2 = "Your_Other_Password";
// void connectWiFi() {
//     Serial.println("Connecting to WiFi 1...");
//     WiFi.begin(ssid1, pass1);

//     int attempts = 0;
//     while (WiFi.status() != WL_CONNECTED && attempts < 10) {
//         delay(500);
//         Serial.print(".");
//         attempts++;
//     }

//     if (WiFi.status() == WL_CONNECTED) {
//         Serial.println("\nConnected to WiFi 1 ✅");
//         return;
//     }

//     // Try second WiFi
//     Serial.println("\nWiFi 1 failed. Trying WiFi 2...");
//     WiFi.begin(ssid2, pass2);

//     attempts = 0;
//     while (WiFi.status() != WL_CONNECTED && attempts < 10) {
//         delay(500);
//         Serial.print(".");
//         attempts++;
//     }

//     if (WiFi.status() == WL_CONNECTED) {
//         Serial.println("\nConnected to WiFi 2 ✅");
//     } else {
//         Serial.println("\nWiFi Failed ❌");
//     }
// }
