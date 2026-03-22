
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClient.h>

// WIFI
const char* ssid = "Airtel_mohd_3792";
const char* password = "Air@28347";

// VERSION
#define CURRENT_VERSION "v1.0.0"

// Supabase
String baseUrl = "https://yljggigahlagdihhycfj.supabase.co";
String firmwareEndpoint = baseUrl + "/rest/v1/firmware?select=version,url&order=id.desc&limit=1";
String anonKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlsamdnaWdhaGxhZ2RpaGh5Y2ZqIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzI3MDcwNTMsImV4cCI6MjA4ODI4MzA1M30.NeGRlQv-T-OGW4iqJPLV2T-2uPQxDNz0r9GHLAG8F-g";

// Ultrasonic pins
const int trigPin = 26;
const int echoPin = 27;

long duration;
int distanceCm;

void checkForUpdate() {
 WiFiClient client;
 String NewfirmwareURL = "https://yljggigahlagdihhycfj.supabase.co/storage/v1/object/public/firmwares/v1.0.2.bin";

Serial.println("Starting OTA...");

t_httpUpdate_return ret = httpUpdate.update(client, NewfirmwareURL);

switch (ret) {
  case HTTP_UPDATE_FAILED:
    Serial.printf("Update failed. Error (%d): %s\n",
      httpUpdate.getLastError(),
      httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("No updates");
    break;

  case HTTP_UPDATE_OK:
    Serial.println("Update success");
    break;
}
}

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
    delay(3000);  // allow power stabilization
    checkForUpdate();
  // 🔥 CHECK OTA ON BOOT
  checkForUpdate();
}

void loop() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distanceCm = duration * 0.034 / 2;

  Serial.print("Distance: ");
  Serial.print(distanceCm);
  Serial.println(" cm");

  delay(1000);
}