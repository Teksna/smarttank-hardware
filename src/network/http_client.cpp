#include "http_client.h"

int HttpClientWrapper::get(String url, String& response, String apiKey) {
    HTTPClient http;

    http.addHeader("Content-Type", "application/json");
    http.begin(url);
    http.addHeader("apikey", apiKey);
    http.addHeader("Authorization", "Bearer " + apiKey);

    int code = http.GET();

    if (code > 0) {
        response = http.getString();
    }

    http.end();
    return code;
}

int HttpClientWrapper::post(String url, String payload, String apiKey) {
    HTTPClient http;

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", apiKey);
    http.addHeader("Authorization", "Bearer " + apiKey);
    http.addHeader("Prefer", "return=minimal");

    int code = http.POST(payload);

    http.end();
    return code;
}
int HttpClientWrapper::patch(String url, String payload, String apiKey) {
    
    HTTPClient http;

    http.begin(url);
    http.setTimeout(5000);
    http.setReuse(true);

    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", apiKey);
    http.addHeader("Authorization", "Bearer " + apiKey);
    http.addHeader("Prefer", "return=minimal");

    int httpCode = http.sendRequest("PATCH", payload);
    //String response = http.getString();

    http.end();

    Serial.println("----- PATCH DEBUG -----");
    Serial.println("URL: " + url);
    Serial.println("Payload: " + payload);
    Serial.println("Response Code: " + String(httpCode));
    //Serial.println("Response Body: " + response);
    Serial.println("------------------------");

    return httpCode;
}