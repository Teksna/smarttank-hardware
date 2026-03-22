#include "ultrasonic.h"
#include <Arduino.h>

static int trig, echo;

void initUltrasonic(int trigPin, int echoPin) {
    trig = trigPin;
    echo = echoPin;

    pinMode(trig, OUTPUT);
    pinMode(echo, INPUT);
}

int getTankCapacity(int tankHeight)
{
    digitalWrite(trig, LOW);
    delayMicroseconds(2);

    digitalWrite(trig, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig, LOW);

    long duration = pulseIn(echo, HIGH);
    int distanceCm = duration * 0.034 / 2;

    float waterLevel = tankHeight - distanceCm;
    int capacity = (waterLevel / tankHeight) * 100;

    if (capacity > 100) capacity = 100;
    if (capacity < 0) capacity = 0;

    return capacity;
}