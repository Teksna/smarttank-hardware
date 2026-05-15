// #include <Arduino.h>
// #include "motor.h"
// #include "utils/config.h"

// bool motorState = false;

// void initMotor() {
//     pinMode(motorPin_no, OUTPUT);
//     pinMode(motorPin_nc, OUTPUT);
//     digitalWrite(motorPin_nc, LOW); // Ensure normally closed relay is active to keep motor off
//     delay(2000);                    // brief delay to ensure relay state change
//     digitalWrite(motorPin_no, LOW); // Ensure normally open relay is inactive to keep motor off
//     Serial.println("Motor Pins Initialized");
//     delay(1000);
// }

// void motorOn() {
//     Serial.println("Motor ON");
//     delay(200);
//     digitalWrite(motorPin_no, HIGH);
//     motorState = true;
// }

// void motorOff() {
//     Serial.println("Motor OFF");
//     digitalWrite(motorPin_no, LOW);
//     motorState = false;
// }