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


CloudResponse OTAupdateAndFetchSupply(int tank, bool motor, int rssi, int battery) {

    CloudResponse result = {false, false};

    if (WiFi.status() != WL_CONNECTED) return result;

    String url = SUPABASE_URL + "/rest/v1/rpc/ota_update_device_and_get_supply";

    String payload = "{";
    payload += "\"p_device_uid\":\"" + DEVICE_ID + "\",";
    payload += "\"p_tank_level\":" + String(tank) + ",";
    payload += "\"p_motor_state\":" + String(motor ? "true" : "false") + ",";
    payload += "\"p_battery_level\":" + String(battery);
    payload += "}";

    String response;
    int code = HttpClientWrapper::post(url, payload, response, SUPABASE_KEY);

    Serial.println("RPC Response: " + response);

    if (code == 200) {

        if (response.indexOf("\"supply_state\":true") >= 0) {
            result.supply = true;
        }

        if (response.indexOf("\"ota_status\":true") >= 0) {
            result.ota = true;
        }
    }

    return result;
}
