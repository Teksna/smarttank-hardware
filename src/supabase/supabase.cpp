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
int extractIntValue(String response, String key) {
    int start = response.indexOf(key);
    if (start == -1) return 90; // default

    start = response.indexOf(":", start);
    int end = response.indexOf(",", start);

    if (end == -1) {
        end = response.indexOf("}", start);
    }

    String value = response.substring(start + 1, end);
    value.trim();

    return value.toInt();
}

CloudResponse ota_device_update_and_fetch(int tank, bool motor, int rssi, int battery) {

    CloudResponse result = {false, false, false};

    if (WiFi.status() != WL_CONNECTED) return result;

    String url = SUPABASE_URL + "/rest/v1/rpc/ota_update_device_and_get_supply_automation_height";

    String payload = "{";
    payload += "\"p_device_uid\":\"" + DEVICE_ID + "\",";
    payload += "\"p_tank_level\":" + String(tank) + ",";
    payload += "\"p_motor_state\":" + String(motor ? "true" : "false") + ",";
    payload += "\"p_battery_level\":" + String(battery) + ",";
    payload += "\"p_rssi\":" + String(rssi);
    payload += "}";

    String response;
    int code = HttpClientWrapper::post(url, payload, response, SUPABASE_KEY);

    Serial.println("RPC Response: " + response);

    if (code == 200) {

    result.supply = response.indexOf("\"supply_state\":true") >= 0;
    result.ota = response.indexOf("\"ota_status\":true") >= 0;
    result.motorAutomation = response.indexOf("\"motor_automation\":true") >= 0;

    result.overflowThreshold = extractIntValue(response, "\"overflow_height\"");
}

    return result;
}
