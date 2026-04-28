#pragma once
#include <Arduino.h>
struct CloudResponse {
    bool supply;
    bool ota;
    bool motorAutomation; 
};

void supabaseInit(const char* url, const char* key);
bool fetchSupplyState();
void updateDeviceState(int tank, bool motor, int rssi);
CloudResponse ota_device_update_and_fetch(int tank, bool motor, int rssi, int battery);

