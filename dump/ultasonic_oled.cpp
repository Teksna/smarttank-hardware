#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Fonts/FreeSans9pt7b.h>
// WIFI
const char* ssid = "Airtel_mohd_3792";
const char* password = "Air@28347";

// VERSION
#define CURRENT_VERSION "v1.0.0"

// Supabase
String baseUrl = "https://yljggigahlagdihhycfj.supabase.co";
String firmwareEndpoint = baseUrl + "/rest/v1/firmware?select=version,url&order=id.desc&limit=1";
String anonKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlsamdnaWdhaGxhZ2RpaGh5Y2ZqIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzI3MDcwNTMsImV4cCI6MjA4ODI4MzA1M30.NeGRlQv-T-OGW4iqJPLV2T-2uPQxDNz0r9GHLAG8F-g";

//OLED
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);
const int tankHeight = 100; // cm (CHANGE THIS)

// Ultrasonic pins
const int trigPin = 26;
const int echoPin = 27;

long duration;
int distanceCm;
int capacity = 0;
// ---------- DISPLAY FUNCTION ----------
void displayStatus(int capacity)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setFont(&FreeSans9pt7b);
  display.setCursor(6,15);

  // Tank outline
  display.drawLine(0, display.height(), 0, 11, SH110X_WHITE);
  display.drawLine(0, display.height()-1, 60, display.height()-1, SH110X_WHITE);
  display.drawLine(60, display.height(), 60, 11, SH110X_WHITE);
  display.drawLine(0, 11, 20, 1, SH110X_WHITE);
  display.fillRect(20, 0, 20, 3,1);
  display.drawLine(40, 1, 60, 11, SH110X_WHITE);
  display.drawLine(1, 11, 59, 11, SH110X_WHITE);

  // Volume header
  display.fillRect(65, 0, 66, 20,1);
  display.setCursor(67, 15);
  display.setTextColor(SH110X_BLACK);
  display.print("Volume");

  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);

  if(capacity < 10) display.setCursor(84, 47);
  else if (capacity < 100) display.setCursor(72, 47);
  else display.setCursor(60, 47);

  display.print(capacity);

  // Fill tank animation
  int tankfill = capacity / 2;  // scale for 64px height
  display.fillRect(2, 62 - tankfill, 57, tankfill, 1);

  display.setTextSize(1);
  display.print("%");

  display.display();
}
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

      WiFiClientSecure client;
      client.setInsecure();  // skip certificate validation 
      httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
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
  // Initialize display
  display.begin(0x3C, true);
  display.clearDisplay();
  display.display();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");

  delay(3000);
  checkForUpdate();   //  only once
}

// ---------- LOOP ----------
void loop() {

  // Trigger ultrasonic
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);

  // Distance in cm
  distanceCm = duration * 0.034 / 2;

  // Convert to tank percentage
  float waterLevel = tankHeight - distanceCm;
  capacity = (waterLevel / tankHeight) * 100;

  // Clamp values (IMPORTANT)
  if (capacity > 100) capacity = 100;
  if (capacity < 0) capacity = 0;

  // Debug
  Serial.print("Distance: ");
  Serial.print(distanceCm);
  Serial.print(" cm | Capacity: ");
  Serial.print(capacity);
  Serial.println(" %");

  // Update display
  displayStatus(capacity);

  delay(500);
}