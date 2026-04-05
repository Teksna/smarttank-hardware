#include <RadioLib.h>

// -------- LoRa Pins --------
#define ss 5
#define rst 14
#define dio0 2
#define dio1 3

// -------- Pins --------
#define batteryin 34
#define echoPin 27
#define trigPin 26

SX1276 radio = new Module(ss, dio0, rst, dio1);

// Divider ratio (220k + 100k)
#define DIVIDER_RATIO 3.2

long duration;

// -------- SETUP --------
void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(batteryin, INPUT);

  // Improve ADC range
  analogSetAttenuation(ADC_11db);

  Serial.print("[SX1276] Initializing ... ");

  int state = radio.begin(
    865.0,   // Frequency
    125.0,   // Bandwidth
    9,       // SF
    5,       // CR
    0x12,    // Sync word
    17,      // Power
    8,       // Preamble
    0        // Gain
  );

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("success!");
  } else {
    Serial.print("failed, code ");
    Serial.println(state);
    while (true);
  }
}

// -------- DISTANCE --------
int readDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH, 30000); // timeout

  int dist = duration * 0.034 / 2;

  if (dist <= 0 || dist > 1000) return -1;

  return dist;
}

// -------- BATTERY RAW (AVERAGED) --------
int readBatteryRaw() {
  int sum = 0;

  for (int i = 0; i < 10; i++) {
    sum += analogRead(batteryin);
    delay(5);
  }

  return sum / 10;
}

// -------- BATTERY VOLTAGE --------
float readBatteryVoltage() {
  int raw = readBatteryRaw();

  float v_adc = raw * (3.3 / 4095.0);

  float v_battery = v_adc * DIVIDER_RATIO;

  return v_battery;
}

// -------- BATTERY % (1–100 SCALE) --------
int batteryPercent(float voltage) {
  float v_min = 6.4;
  float v_max = 8.4;

  float percent = ((voltage - v_min) / (v_max - v_min)) * 100.0;

  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;

  return (int)(percent + 0.5); // round
}

// -------- LOOP --------
void loop() {

  Serial.println("Transmitting...");

  int distance = readDistance();
  float battery = readBatteryVoltage();
  int percent = batteryPercent(battery);

  Serial.print("Distance: ");
  Serial.println(distance);

  Serial.print("Battery Voltage: ");
  Serial.println(battery);

  Serial.print("Battery %: ");
  Serial.println(percent);

  // -------- Payload --------
  String payload = String(distance) + "," +
                   String(battery, 2) + "," +
                   String(percent);

  Serial.print("Payload: ");
  Serial.println(payload);

  int state = radio.transmit(payload);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("LoRa TX success!");
  } else {
    Serial.print("failed, code ");
    Serial.println(state);
  }

  Serial.println("----------------------");

  delay(5000);  // change later to deep sleep
}