#include <Arduino.h>
#include <LoRaLib.h>
#include "sensor/ultrasonic.h"

// LORA
#define ss 5
#define rst 14
#define dio0 2
#define dio1 34

SX1276 lora = new LoRa(ss, dio0, dio1);

// CONFIG
String TXID = "T001";
const int tankHeight = 100;

// PINS
const int trigPin = 26;
const int echoPin = 27;

void setup()
{
    Serial.begin(115200);

    initUltrasonic(trigPin, echoPin);

    Serial.print("Initializing LoRa... ");

    int state = lora.begin(865.0, 125.0, 9, 5, 0x12, 17, 100, 8, 6);

    if (state == ERR_NONE)
        Serial.println("OK");
    else
    {
        Serial.println("FAIL");
        while (true);
    }
}

void loop()
{
    int capacity = getTankCapacity(tankHeight);

    String payload = TXID + "|" + String(capacity);

    Serial.println("Sending: " + payload);

    lora.transmit(payload);

    delay(2000);
}