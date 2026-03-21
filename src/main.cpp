#define CURRENT_VERSION "v1.0.0"
#include <Arduino.h>

// Defines the trigger and echo pins
const int trigPin = 26;
const int echoPin = 27;

// Variables for duration and distance
long duration;
int distanceCm;


void setup() {
  // Initialize serial communication at 9600 baud rate
  Serial.begin(9600);
  // Define pins as output and input
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
}

void loop() {
  // Clear the trigPin by setting it LOW for a moment
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Send a 10 microsecond HIGH pulse to the trigPin to trigger the sensor
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);

  // Calculate the distance: Speed of sound is 0.034 cm/us (or 340 m/s).
  // The duration is the time to travel to the object and back, so divide by 2.
  distanceCm = duration * 0.034 / 2;

  // Print the distance to the Serial Monitor
  Serial.print("Distance: ");
  Serial.print(distanceCm);
  Serial.println(" cm");

  // Add a small delay between measurements
  delay(100);
}
