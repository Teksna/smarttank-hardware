#include "ota.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>

// 🔁 Move these from main.cpp
#define CURRENT_VERSION "v1.0.1"

static String baseUrl = "https://yljggigahlagdihhycfj.supabase.co";
static String firmwareEndpoint = baseUrl + "/rest/v1/firmware?select=version,url&order=id.desc&limit=1";

String anonKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlsamdnaWdhaGxhZ2RpaGh5Y2ZqIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzI3MDcwNTMsImV4cCI6MjA4ODI4MzA1M30.NeGRlQv-T-OGW4iqJPLV2T-2uPQxDNz0r9GHLAG8F-g";

// ---------- INTERNAL JSON PARSER ----------
static String extractValue(String payload, String key) {
    int start = payload.indexOf(key);
    if (start == -1) return "";

    start = payload.indexOf(":", start) + 2;
    int end = payload.indexOf("\"", start);

    return payload.substring(start, end);
}

// ---------- OTA FUNCTION ----------
void checkForOTAUpdate() {
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
            client.setInsecure();

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