#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClient.h>

// WIFI
const char* ssid = "Airtel_mohd_3792";
const char* password = "Air@28347";

// VERSION
#define CURRENT_VERSION "v1.0.1"

// Supabase
String baseUrl = "https://yljggigahlagdihhycfj.supabase.co";
String firmwareEndpoint = baseUrl + "/rest/v1/firmware?select=version,url&order=id.desc&limit=1";
String anonKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlsamdnaWdhaGxhZ2RpaGh5Y2ZqIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzI3MDcwNTMsImV4cCI6MjA4ODI4MzA1M30.NeGRlQv-T-OGW4iqJPLV2T-2uPQxDNz0r9GHLAG8F-g";


// Ultrasonic pins
const int trigPin = 26;
const int echoPin = 27;

long duration;
int distanceCm;

// 🔥 Extract value from JSON (simple parser)
String extractValue(String payload, String key) {
  int start = payload.indexOf(key);
  if (start == -1) return "";

  start = payload.indexOf(":", start) + 2;
  int end = payload.indexOf("\"", start);

  return payload.substring(start, end);
}

void checkForUpdate() {
  HTTPClient http;

  Serial.println("Checking for firmware...");

  http.begin(firmwareEndpoint);
  http.addHeader("apikey", anonKey);

  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();

    Serial.println("Response:");
    Serial.println(payload);

    String latestVersion = extractValue(payload, "version");
    String firmwareURL = extractValue(payload, "url");

    Serial.print("Latest Version: ");
    Serial.println(latestVersion);

    Serial.print("Current Version: ");
    Serial.println(CURRENT_VERSION);

    if (latestVersion != CURRENT_VERSION) {
      Serial.println("New firmware found!");

      WiFiClient client;
      t_httpUpdate_return ret = httpUpdate.update(client, firmwareURL);

      switch (ret) {
        case HTTP_UPDATE_FAILED:
          Serial.printf("Update failed (%d): %s\n",
            httpUpdate.getLastError(),
            httpUpdate.getLastErrorString().c_str());
          break;

        case HTTP_UPDATE_NO_UPDATES:
          Serial.println("No updates");
          break;

        case HTTP_UPDATE_OK:
          Serial.println("Update success!");
          break;
      }
    } else {
      Serial.println("Already up to date");
    }

  } else {
    Serial.printf("HTTP error: %d\n", httpCode);
  }

  http.end();
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

  delay(3000);
  checkForUpdate();   // 🔥 only once
}

void loop() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distanceCm = duration * 0.034 / 2;

  Serial.print("OTA Distance: ");
  Serial.print(distanceCm);
  Serial.println(" cm");

  delay(1000);
}