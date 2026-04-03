
#include "ota.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include "../network/http_client.h"

// ---------------- CONFIG ----------------
#define CURRENT_VERSION "v1.0.0"

static String baseUrl = "https://yljggigahlagdihhycfj.supabase.co";
static String firmwareEndpoint = baseUrl + "/rest/v1/firmware?select=version,url&order=id.desc&limit=1";

static String anonKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlsamdnaWdhaGxhZ2RpaGh5Y2ZqIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzI3MDcwNTMsImV4cCI6MjA4ODI4MzA1M30.NeGRlQv-T-OGW4iqJPLV2T-2uPQxDNz0r9GHLAG8F-g";
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

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi not connected. Skipping OTA.");
        return;
    }

    Serial.println("Checking for firmware...");

    String payload;
    int httpCode = HttpClientWrapper::get(firmwareEndpoint, payload, anonKey);

    if (httpCode == 200) {

        Serial.println("Response:");
        Serial.println(payload);

        String latestVersion = extractValue(payload, "version");
        String firmwareURL  = extractValue(payload, "url");

        Serial.print("Latest Version: ");
        Serial.println(latestVersion);

        Serial.print("Current Version: ");
        Serial.println(CURRENT_VERSION);

        if (latestVersion.length() == 0 || firmwareURL.length() == 0) {
            Serial.println("Invalid firmware data");
            return;
        }

        if (latestVersion != CURRENT_VERSION) {

            Serial.println("New firmware found!");

            WiFiClientSecure client;
            client.setInsecure();              // ⚠️ for HTTPS without cert
            client.setTimeout(15000);          // prevent hangs

            httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

            t_httpUpdate_return ret = httpUpdate.update(client, firmwareURL);

            switch (ret) {

                case HTTP_UPDATE_FAILED:
                    Serial.printf("Update failed (%d): %s\n",
                        httpUpdate.getLastError(),
                        httpUpdate.getLastErrorString().c_str());
                    break;

                case HTTP_UPDATE_NO_UPDATES:
                    Serial.println("No updates available");
                    break;

                case HTTP_UPDATE_OK:
                    Serial.println("Update success! Rebooting...");
                    break;
            }

        } else {
            Serial.println("Already up to date");
        }

    } else {
        Serial.print("HTTP error: ");
        Serial.println(httpCode);
    }
}