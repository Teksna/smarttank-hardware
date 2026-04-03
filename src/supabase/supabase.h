#pragma once
#include <Arduino.h>

void supabaseInit(const char* url, const char* key);
bool fetchSupplyState();
void updateDeviceState(int tank, bool motor, int rssi);
bool updateAndFetchSupply(int tank, bool motor, int rssi);
