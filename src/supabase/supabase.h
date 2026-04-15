#pragma once
#include <Arduino.h>
struct CloudResponse {
    bool supply;
    bool ota;
};

void supabaseInit(const char* url, const char* key);
bool fetchSupplyState();
void updateDeviceState(int tank, bool motor, int rssi);
CloudResponse OTAupdateAndFetchSupply(int tank, bool motor, int rssi, int battery);

