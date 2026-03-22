#ifndef ULTRASONIC_H
#define ULTRASONIC_H

void initUltrasonic(int trigPin, int echoPin);
int getTankCapacity(int tankHeight);

#endif