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
#define DIVIDER_RATIO  3.93

// -------- Distance Limits --------
#define MAX_DISTANCE 120
#define MIN_DISTANCE 19          // sensor unreliable below this
#define MIN_VALID_DISTANCE 19    // clamp prediction here

long duration;

// -------- Prediction Variables --------
int lastDistance = 0;
int lastRate = 0;
int lastValidDistance = 0;

// -------- SETUP --------
void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(batteryin, INPUT);

  digitalWrite(trigPin, LOW);

  analogSetAttenuation(ADC_11db);

  Serial.print("[SX1276] Initializing ... ");

  int state = radio.begin(
    865.0,
    125.0,
    9,
    5,
    0x12,
    17,
    8,
    0
  );

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("success!");
  } else {
    Serial.print("failed, code ");
    Serial.println(state);
    while (true);
  }
}

// -------- RAW DISTANCE --------
int readDistanceRaw() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(3);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);

  if (duration == 0) return -1;

  int dist = duration * 0.0343 / 2;

  if (dist < MIN_DISTANCE || dist > MAX_DISTANCE) return -1;

  return dist;
}

// -------- PREDICTION FILTER --------
int applyPrediction(int newValue) {

  if (lastDistance == 0) {
    lastDistance = newValue;
    return newValue;
  }

  int currentRate = newValue - lastDistance;

  int smoothRate = (lastRate * 0.7) + (currentRate * 0.3);

  // Clamp unrealistic speed
  if (smoothRate > 5) smoothRate = 5;
  if (smoothRate < -5) smoothRate = -5;

  int predicted = lastDistance + smoothRate;

  // 🔥 Clamp to minimum valid distance
  if (predicted < MIN_VALID_DISTANCE) {
    predicted = MIN_VALID_DISTANCE;
  }

  int diff = abs(newValue - predicted);

  // Reject anomaly
  if (diff > 20) {
    Serial.println("⚠️ Using predicted value");

    lastDistance = predicted;
    lastRate = smoothRate;
    return predicted;
  }

  lastDistance = newValue;
  lastRate = smoothRate;

  return newValue;
}

// -------- STABLE DISTANCE --------
int readDistance() {
  const int samples = 5;
  int readings[samples];
  int count = 0;

  for (int i = 0; i < samples; i++) {
    int d = readDistanceRaw();
    Serial.print("Raw reading: ");
    Serial.println(d);
    if (d != -1) {
      readings[count++] = d;
    }

    delay(30);
  }

  // -------- No echo → predict --------
  // 
  if (count == 0) {
    Serial.println("⚠️ No echo");

    return MIN_VALID_DISTANCE;
}
  

  // -------- Sort for median --------
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (readings[j] < readings[i]) {
        int temp = readings[i];
        readings[i] = readings[j];
        readings[j] = temp;
      }
    }
  }

  int median = readings[count / 2];

  int finalValue = applyPrediction(median);

  lastValidDistance = finalValue;
  Serial.print("Final distance: ");
  Serial.println(finalValue);
  return finalValue;
}

// -------- BATTERY RAW --------
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
  Serial.print("ADC Raw: ");
  Serial.println(raw);

  float v_adc = raw * (3.3 / 4095.0);
  Serial.print("ADC Voltage: ");
  Serial.println(v_adc, 3);

  float v_battery = v_adc * DIVIDER_RATIO;

  return v_battery;
}

// -------- BATTERY % --------
int batteryPercent(float voltage) {
  float v_min = 5.5;
  float v_max = 6.4;

  float percent = ((voltage - v_min) / (v_max - v_min)) * 100.0;

  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;

  return (int)(percent + 0.5);
}

// -------- LOOP --------
void loop() {

  Serial.println("Transmitting...");

  int distance = readDistance();
  float battery = readBatteryVoltage();
  int percent = batteryPercent(battery);

  // Serial.print("Distance: ");
  // Serial.println(distance);

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

  delay(5000);
}