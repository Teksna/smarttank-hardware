#include "supabase.h"
#include "../network/http_client.h"
#include <WiFi.h>

// static String STATION_ID = "02f576be-835b-4e4e-92b4-fc5efbac6749";
static String SUPABASE_URL;
static String SUPABASE_KEY;

static bool cloudSupply = false;
static String DEVICE_ID = "tank1";  // must match device_uid in DB

void supabaseInit(const char* url, const char* key) {
    SUPABASE_URL = String(url);
    SUPABASE_KEY = String(key);
}

bool updateAndFetchSupply(int tank, bool motor, int rssi) {

    if (WiFi.status() != WL_CONNECTED) return false;

    String url = SUPABASE_URL + "/rest/v1/rpc/update_device_and_get_supply";

    String payload = "{";
    payload += "\"p_device_uid\":\"" + DEVICE_ID + "\",";
    payload += "\"p_tank_level\":" + String(tank) + ",";
    payload += "\"p_motor_state\":" + String(motor ? "true" : "false");
    payload += "}";

    String response;
    int code = HttpClientWrapper::post(url, payload, response, SUPABASE_KEY);

    Serial.println("RPC Response: " + response);

    if (code == 200 && response.indexOf("\"supply_state\":true") >= 0) {
        return true;
    }

    return false;
}
// // 🔹 Fetch supply_state from devices table
// bool fetchSupplyState() {

//     if (WiFi.status() != WL_CONNECTED) return cloudSupply;

//     String response;

//     String url = SUPABASE_URL +
//   "/rest/v1/supply_stations?id=eq." + STATION_ID +
//   "&select=supply_state&limit=1";

//     int code = HttpClientWrapper::get(url, response, SUPABASE_KEY);
//     // get the result and update cloudSupply
//     Serial.println("HTTP GET response code: " + String(code));
//     // extract response
//     Serial.println("HTTP GET response: " + response);
    
//     if (code == 200 && response.length() > 0) {
//         cloudSupply = (response.indexOf("\"supply_state\":true") >= 0);
//     }

//     Serial.println("Fetched supply state: " + String(cloudSupply ? "true" : "false"));
//     return cloudSupply;
// }

// // 🔹 Update device (THIS triggers logs automatically)
// void updateDeviceState(int tank, bool motor, int rssi) {

//     if (WiFi.status() != WL_CONNECTED) return;

//     String url = SUPABASE_URL +
//         "/rest/v1/devices?device_uid=eq." + DEVICE_ID;

//     String payload = "{";
//     payload += "\"tank_level_percent\":" + String(tank) + ",";
//     payload += "\"motor_state\":" + String(motor ? "true" : "false");
//     payload += "}";

//     HttpClientWrapper::patch(url, payload, SUPABASE_KEY);
//         //Serial.println("HTTP PATCH payload: " + payload);
//         //
// }