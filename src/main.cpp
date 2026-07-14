#include <Arduino.h>
#include <RadioLib.h>
#include <esp_sleep.h>

// -------- Debug --------
constexpr bool DEBUG_SERIAL = true;

#define DEBUG_PRINT(...) do { if (DEBUG_SERIAL) Serial.print(__VA_ARGS__); } while (0)
#define DEBUG_PRINTLN(...) do { if (DEBUG_SERIAL) Serial.println(__VA_ARGS__); } while (0)

// -------- LoRa Pins --------
constexpr int ss = 5;
constexpr int rst = 14;
constexpr int dio0 = 2;
constexpr int dio1 = 3;

// -------- Pins --------
constexpr int batteryin = 34;
constexpr int echoPin = 27;
constexpr int trigPin = 26;

Module loraModule(ss, dio0, rst, dio1);
SX1276 radio(&loraModule);

// Divider ratio (220k + 100k), calibrated from measured 6.40 V / 1.822 V ADC.
constexpr float DIVIDER_RATIO = 3.51f;

// -------- Battery Limits --------
constexpr float BATTERY_EMPTY_V = 5.50f;
constexpr float BATTERY_LOW_ENTER_V = 5.70f;
constexpr float BATTERY_LOW_EXIT_V = 5.90f;
constexpr float BATTERY_FULL_V = 6.40f;
constexpr float BATTERY_MIN_SANITY_V = 4.80f;
constexpr float BATTERY_MAX_SANITY_V = 6.90f;

// -------- Distance Limits --------
constexpr int MAX_DISTANCE = 120;
constexpr int MIN_DISTANCE = 19;          // sensor unreliable below this
constexpr int MIN_VALID_DISTANCE = 19;    // clamp prediction here

// -------- Distance Filter Settings --------
constexpr int RAW_SAMPLE_COUNT = 9;
constexpr int MIN_SAMPLES_AFTER_MAD = 3;
constexpr uint32_t ECHO_TIMEOUT_US = 30000UL;
constexpr uint16_t SAMPLE_SPACING_MS = 25;
constexpr float DISTANCE_EMA_ALPHA = 0.35f;
constexpr float DISTANCE_PREDICT_EMA_ALPHA = 0.18f;
constexpr int MAX_FAILURES_BEFORE_STALE = 6;
constexpr int SENSOR_FAULT_FAILURES = 12;
constexpr int UNRELIABLE_ENTER_CM = 20;
constexpr int UNRELIABLE_EXIT_CM = 22;

// -------- Battery Filter Settings --------
constexpr int BATTERY_SAMPLE_COUNT = 15;
constexpr int BATTERY_TRIM_COUNT = 2;
constexpr uint16_t BATTERY_SAMPLE_DELAY_MS = 3;
constexpr float BATTERY_EMA_ALPHA = 0.25f;
constexpr float BATTERY_MAX_STEP_V = 0.35f;

// -------- Timing --------
constexpr uint64_t TRANSMIT_INTERVAL_US = 5000000ULL;
constexpr uint32_t RTC_MAGIC = 0x534D5458UL;

struct DistanceState {
  int lastDistance;
  int lastRate;
  int lastValidDistance;
  int consecutiveFailures;
  int sensorConfidence;
  bool emaInitialized;
  float emaDistance;
  bool unreliableRegionLatched;
};

struct BatteryState {
  bool emaInitialized;
  bool lowBatteryLatched;
  bool adcHealthy;
  int health;
  float emaVoltage;
  float lastValidVoltage;
};

RTC_DATA_ATTR uint32_t rtcMagic = 0;
RTC_DATA_ATTR DistanceState distanceState;
RTC_DATA_ATTR BatteryState batteryState;

struct DistanceResult {
  int distanceCm;
  int medianCm;
  int madCm;
  int acceptedSamples;
  int validSamples;
  int timeouts;
  int confidence;
  bool usedPrediction;
  bool sensorFault;
};

struct BatteryResult {
  int rawAdc;
  float adcVoltage;
  float batteryVoltage;
  int percent;
  bool lowBattery;
  bool adcHealthy;
  int health;
};

struct SocPoint {
  float voltage;
  int percent;
};

// Conservative 2S LiFePO4 open-circuit SoC curve. Under load, readings may sit
// a little lower, so the EMA and hysteresis prevent fast percentage chatter.
constexpr SocPoint LIFEPO4_SOC_TABLE[] = {
  {5.50f, 0},
  {5.70f, 5},
  {5.90f, 10},
  {6.00f, 20},
  {6.10f, 35},
  {6.20f, 55},
  {6.28f, 75},
  {6.34f, 90},
  {6.40f, 100}
};

// -------- Helpers --------
template <typename T>
T clampValue(T value, T low, T high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

void initializeRtcStateIfNeeded() {
  if (rtcMagic == RTC_MAGIC) {
    return;
  }

  rtcMagic = RTC_MAGIC;

  distanceState.lastDistance = 0;
  distanceState.lastRate = 0;
  distanceState.lastValidDistance = 0;
  distanceState.consecutiveFailures = 0;
  distanceState.sensorConfidence = 0;
  distanceState.emaInitialized = false;
  distanceState.emaDistance = 0.0f;
  distanceState.unreliableRegionLatched = false;

  batteryState.emaInitialized = false;
  batteryState.lowBatteryLatched = false;
  batteryState.adcHealthy = true;
  batteryState.health = 100;
  batteryState.emaVoltage = 0.0f;
  batteryState.lastValidVoltage = 0.0f;
}

void sortIntReadings(int values[], int count) {
  for (int i = 1; i < count; i++) {
    int key = values[i];
    int j = i - 1;

    while (j >= 0 && values[j] > key) {
      values[j + 1] = values[j];
      j--;
    }

    values[j + 1] = key;
  }
}

void sortUint16Readings(uint16_t values[], int count) {
  for (int i = 1; i < count; i++) {
    uint16_t key = values[i];
    int j = i - 1;

    while (j >= 0 && values[j] > key) {
      values[j + 1] = values[j];
      j--;
    }

    values[j + 1] = key;
  }
}

int medianOfSorted(const int values[], int count) {
  if (count <= 0) return -1;

  if ((count % 2) == 0) {
    return (values[(count / 2) - 1] + values[count / 2]) / 2;
  }

  return values[count / 2];
}

// -------- Ultrasonic Sensor --------
int readDistanceRaw(bool *timedOut) {
  if (timedOut != nullptr) {
    *timedOut = false;
  }

  digitalWrite(trigPin, LOW);
  delayMicroseconds(3);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Timeout bounds the blocking pulse measurement so failed echo cycles cannot
  // hold the firmware awake indefinitely.
  unsigned long duration = pulseIn(echoPin, HIGH, ECHO_TIMEOUT_US);

  if (duration == 0) {
    if (timedOut != nullptr) {
      *timedOut = true;
    }
    return -1;
  }

  int distance = (int)((duration * 0.0343f) / 2.0f + 0.5f);

  // Physical range validation removes impossible echoes before statistics.
  if (distance < MIN_DISTANCE || distance > MAX_DISTANCE) {
    return -1;
  }

  return distance;
}

int adaptiveOutlierLimit(int centre) {
  // Near FULL, false close echoes are most harmful, so the acceptance band is
  // tight. Farther away, a wider band tolerates normal acoustic variation.
  if (centre <= 30) return 5;
  if (centre <= 60) return 7;
  if (centre <= 90) return 10;
  return 12;
}

int rateLimitForDistance(int distance) {
  // Tank level cannot move many centimetres in one five-second cycle. The
  // allowance expands slightly at larger distances for geometry and refill
  // variation.
  if (distance <= 30) return 3;
  if (distance <= 70) return 5;
  return 8;
}

int applyUnreliableRegionHysteresis(int value) {
  // The sensor is unreliable below 19 cm. This latch avoids bouncing between
  // 19 cm and nearby values while the water level sits near FULL.
  if (distanceState.unreliableRegionLatched) {
    if (value >= UNRELIABLE_EXIT_CM) {
      distanceState.unreliableRegionLatched = false;
    } else {
      return MIN_VALID_DISTANCE;
    }
  } else if (value <= UNRELIABLE_ENTER_CM) {
    distanceState.unreliableRegionLatched = true;
    return MIN_VALID_DISTANCE;
  }

  return value;
}

void updateSensorHealth(int validRawCount, int acceptedCount, bool usedPrediction, bool totalLoss) {
  // Confidence is diagnostic only; packet format remains distance,battery,%.
  int confidence = 0;

  confidence += validRawCount * 8;
  confidence += acceptedCount * 5;

  if (usedPrediction) confidence -= 20;
  if (totalLoss) confidence -= 35;
  confidence -= distanceState.consecutiveFailures * 8;

  distanceState.sensorConfidence = clampValue(confidence, 0, 100);
}

int applyPrediction(int newValue, bool *usedPrediction) {
  if (usedPrediction != nullptr) {
    *usedPrediction = false;
  }

  if (distanceState.lastDistance == 0) {
    distanceState.lastDistance = newValue;
    distanceState.lastValidDistance = newValue;
    return newValue;
  }

  int currentRate = newValue - distanceState.lastDistance;
  int smoothRate = (int)((distanceState.lastRate * 0.7f) + (currentRate * 0.3f));
  int maxRate = rateLimitForDistance(distanceState.lastDistance);

  smoothRate = clampValue(smoothRate, -maxRate, maxRate);

  int predicted = distanceState.lastDistance + smoothRate;
  predicted = clampValue(predicted, MIN_VALID_DISTANCE, MAX_DISTANCE);

  int diff = abs(newValue - predicted);
  int jumpLimit = adaptiveOutlierLimit(distanceState.lastDistance) + maxRate;

  // Reject a cycle-level jump that is larger than the tank can plausibly move.
  if (diff > jumpLimit) {
    DEBUG_PRINTLN("Using predicted value; jump rejected");

    distanceState.lastDistance = predicted;
    distanceState.lastRate = smoothRate;
    if (usedPrediction != nullptr) {
      *usedPrediction = true;
    }
    return predicted;
  }

  distanceState.lastDistance = newValue;
  distanceState.lastRate = smoothRate;
  distanceState.lastValidDistance = newValue;

  return newValue;
}

int predictFromHistory() {
  // Temporary echo loss uses the previous trend instead of immediately
  // reporting the unreliable 19 cm boundary.
  if (distanceState.lastDistance == 0) {
    return MIN_VALID_DISTANCE;
  }

  int maxRate = rateLimitForDistance(distanceState.lastDistance);
  int rate = clampValue(distanceState.lastRate, -maxRate, maxRate);
  int predicted = distanceState.lastDistance + rate;

  predicted = clampValue(predicted, MIN_VALID_DISTANCE, MAX_DISTANCE);
  distanceState.lastDistance = predicted;

  return predicted;
}

int applyDistanceEma(int value, bool usedPrediction) {
  // EMA damps turbulence and false echoes across wake cycles. Predicted values
  // use a smaller alpha so stale estimates do not dominate recovery.
  float alpha = usedPrediction ? DISTANCE_PREDICT_EMA_ALPHA : DISTANCE_EMA_ALPHA;

  if (!distanceState.emaInitialized) {
    distanceState.emaDistance = value;
    distanceState.emaInitialized = true;
  } else {
    distanceState.emaDistance =
      (distanceState.emaDistance * (1.0f - alpha)) + (value * alpha);
  }

  int rounded = (int)(distanceState.emaDistance + 0.5f);
  return clampValue(rounded, MIN_VALID_DISTANCE, MAX_DISTANCE);
}

DistanceResult readDistance() {
  int readings[RAW_SAMPLE_COUNT];
  int deviations[RAW_SAMPLE_COUNT];
  int validCount = 0;
  int timeouts = 0;

  for (int i = 0; i < RAW_SAMPLE_COUNT; i++) {
    bool timedOut = false;
    int reading = readDistanceRaw(&timedOut);

    DEBUG_PRINT("Raw reading: ");
    DEBUG_PRINTLN(reading);

    if (timedOut) {
      timeouts++;
    }

    if (reading != -1) {
      readings[validCount++] = reading;
    }

    delay(SAMPLE_SPACING_MS);
  }

  DistanceResult result;
  result.distanceCm = MIN_VALID_DISTANCE;
  result.medianCm = -1;
  result.madCm = -1;
  result.acceptedSamples = 0;
  result.validSamples = validCount;
  result.timeouts = timeouts;
  result.confidence = 0;
  result.usedPrediction = false;
  result.sensorFault = false;

  // -------- No echo -> predict --------
  if (validCount == 0) {
    distanceState.consecutiveFailures++;
    result.usedPrediction = true;

    int predicted = predictFromHistory();
    int stable = applyDistanceEma(predicted, true);
    stable = applyUnreliableRegionHysteresis(stable);

    // After repeated losses, hold the last known stable value before declaring
    // a persistent sensor fault. This avoids sudden false FULL packets.
    if (distanceState.consecutiveFailures > MAX_FAILURES_BEFORE_STALE &&
        distanceState.lastValidDistance > 0) {
      stable = distanceState.lastValidDistance;
    }

    result.sensorFault = distanceState.consecutiveFailures >= SENSOR_FAULT_FAILURES;
    result.distanceCm = stable;

    updateSensorHealth(0, 0, true, true);
    result.confidence = distanceState.sensorConfidence;

    DEBUG_PRINTLN("No echo; using graceful prediction");
    return result;
  }

  // -------- Median filter --------
  sortIntReadings(readings, validCount);
  int median = medianOfSorted(readings, validCount);

  // -------- MAD outlier rejection --------
  for (int i = 0; i < validCount; i++) {
    deviations[i] = abs(readings[i] - median);
  }

  sortIntReadings(deviations, validCount);
  int mad = medianOfSorted(deviations, validCount);
  int madLimit = mad * 3;
  int adaptiveLimit = adaptiveOutlierLimit(median);

  if (madLimit < adaptiveLimit) {
    madLimit = adaptiveLimit;
  }

  long sum = 0;
  int accepted = 0;

  for (int i = 0; i < validCount; i++) {
    if (abs(readings[i] - median) <= madLimit) {
      sum += readings[i];
      accepted++;
    }
  }

  // If too few readings survive, the median is safer than a tiny average.
  int averaged = median;
  if (accepted >= MIN_SAMPLES_AFTER_MAD) {
    averaged = (int)((sum + (accepted / 2)) / accepted);
  }

  bool weakCycle = (validCount < MIN_SAMPLES_AFTER_MAD) ||
                   (accepted < MIN_SAMPLES_AFTER_MAD);
  if (weakCycle) {
    distanceState.consecutiveFailures++;
  } else {
    distanceState.consecutiveFailures = 0;
  }

  bool usedPrediction = false;
  int predictionChecked = applyPrediction(averaged, &usedPrediction);
  int stable = applyDistanceEma(predictionChecked, usedPrediction);
  int finalValue = applyUnreliableRegionHysteresis(stable);

  distanceState.lastValidDistance = finalValue;
  updateSensorHealth(validCount, accepted, usedPrediction, false);

  result.distanceCm = finalValue;
  result.medianCm = median;
  result.madCm = mad;
  result.acceptedSamples = accepted;
  result.confidence = distanceState.sensorConfidence;
  result.usedPrediction = usedPrediction;
  result.sensorFault = distanceState.consecutiveFailures >= SENSOR_FAULT_FAILURES;

  return result;
}

// -------- Battery Measurement --------
uint16_t readBatteryAdcFiltered() {
  uint16_t samples[BATTERY_SAMPLE_COUNT];

  // One discarded conversion lets the ADC sampling capacitor settle after wake.
  (void)analogRead(batteryin);
  delay(BATTERY_SAMPLE_DELAY_MS);

  for (int i = 0; i < BATTERY_SAMPLE_COUNT; i++) {
    samples[i] = (uint16_t)analogRead(batteryin);
    delay(BATTERY_SAMPLE_DELAY_MS);
  }

  sortUint16Readings(samples, BATTERY_SAMPLE_COUNT);

  uint32_t sum = 0;
  int count = 0;

  // Trim both tails to remove ADC spikes before averaging.
  for (int i = BATTERY_TRIM_COUNT; i < BATTERY_SAMPLE_COUNT - BATTERY_TRIM_COUNT; i++) {
    sum += samples[i];
    count++;
  }

  if (count <= 0) {
    return samples[BATTERY_SAMPLE_COUNT / 2];
  }

  return (uint16_t)((sum + (count / 2)) / count);
}

float adcRawToVoltage(uint16_t raw) {
  // analogReadMilliVolts() is available in ESP32 Arduino and uses board ADC
  // calibration when present. Scale the filtered raw value through the same
  // 12-bit range so we retain trimmed-mean filtering.
  uint32_t pinMilliVolts = analogReadMilliVolts(batteryin);

  if (pinMilliVolts > 0 && pinMilliVolts < 3300UL) {
    float calibratedPinVolts = pinMilliVolts / 1000.0f;
    uint16_t instantRaw = (uint16_t)analogRead(batteryin);

    if (instantRaw > 0) {
      return calibratedPinVolts * ((float)raw / (float)instantRaw);
    }
  }

  return raw * (3.3f / 4095.0f);
}

float applyBatteryEma(float voltage, bool validVoltage) {
  if (!validVoltage) {
    return batteryState.emaInitialized ? batteryState.emaVoltage : 0.0f;
  }

  if (!batteryState.emaInitialized) {
    batteryState.emaVoltage = voltage;
    batteryState.emaInitialized = true;
  } else {
    batteryState.emaVoltage =
      (batteryState.emaVoltage * (1.0f - BATTERY_EMA_ALPHA)) +
      (voltage * BATTERY_EMA_ALPHA);
  }

  batteryState.lastValidVoltage = batteryState.emaVoltage;
  return batteryState.emaVoltage;
}

int estimateLiFePo4Soc(float voltage) {
  constexpr int pointCount = sizeof(LIFEPO4_SOC_TABLE) / sizeof(LIFEPO4_SOC_TABLE[0]);

  if (voltage <= LIFEPO4_SOC_TABLE[0].voltage) {
    return LIFEPO4_SOC_TABLE[0].percent;
  }

  if (voltage >= LIFEPO4_SOC_TABLE[pointCount - 1].voltage) {
    return LIFEPO4_SOC_TABLE[pointCount - 1].percent;
  }

  for (int i = 1; i < pointCount; i++) {
    const SocPoint lower = LIFEPO4_SOC_TABLE[i - 1];
    const SocPoint upper = LIFEPO4_SOC_TABLE[i];

    if (voltage <= upper.voltage) {
      float span = upper.voltage - lower.voltage;
      if (span <= 0.0f) {
        return lower.percent;
      }

      float ratio = (voltage - lower.voltage) / span;
      float percent = lower.percent + (ratio * (upper.percent - lower.percent));
      return clampValue((int)(percent + 0.5f), 0, 100);
    }
  }

  return 0;
}

void updateBatteryHealth(float voltage, bool validVoltage, bool rejectedJump) {
  int health = 100;

  if (!validVoltage) health -= 45;
  if (rejectedJump) health -= 25;
  if (batteryState.lowBatteryLatched) health -= 15;

  if (voltage < BATTERY_MIN_SANITY_V || voltage > BATTERY_MAX_SANITY_V) {
    health -= 30;
  }

  batteryState.health = clampValue(health, 0, 100);
  batteryState.adcHealthy = batteryState.health >= 50;
}

BatteryResult readBattery() {
  uint16_t raw = readBatteryAdcFiltered();
  float adcVoltage = adcRawToVoltage(raw);
  float measuredBattery = adcVoltage * DIVIDER_RATIO;

  bool saneVoltage = measuredBattery >= BATTERY_MIN_SANITY_V &&
                     measuredBattery <= BATTERY_MAX_SANITY_V;
  bool rejectedJump = false;

  // A battery cannot jump sharply in one cycle. Reject jumps but keep the
  // previous filtered value so one bad ADC burst does not corrupt the payload.
  if (batteryState.emaInitialized &&
      fabsf(measuredBattery - batteryState.emaVoltage) > BATTERY_MAX_STEP_V) {
    rejectedJump = true;
    saneVoltage = false;
  }

  float filteredBattery = applyBatteryEma(measuredBattery, saneVoltage);

  if (!batteryState.emaInitialized && !saneVoltage) {
    filteredBattery = clampValue(measuredBattery, BATTERY_EMPTY_V, BATTERY_FULL_V);
  }

  if (batteryState.lowBatteryLatched) {
    if (filteredBattery >= BATTERY_LOW_EXIT_V) {
      batteryState.lowBatteryLatched = false;
    }
  } else if (filteredBattery <= BATTERY_LOW_ENTER_V) {
    batteryState.lowBatteryLatched = true;
  }

  updateBatteryHealth(filteredBattery, saneVoltage, rejectedJump);

  BatteryResult result;
  result.rawAdc = raw;
  result.adcVoltage = adcVoltage;
  result.batteryVoltage = filteredBattery;
  result.percent = estimateLiFePo4Soc(filteredBattery);
  result.lowBattery = batteryState.lowBatteryLatched;
  result.adcHealthy = batteryState.adcHealthy;
  result.health = batteryState.health;

  return result;
}

// -------- Radio --------
bool initializeRadio() {
  DEBUG_PRINT("[SX1276] Initializing ... ");

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
    DEBUG_PRINTLN("success!");
    return true;
  }

  DEBUG_PRINT("failed, code ");
  DEBUG_PRINTLN(state);
  return false;
}

void enterDeepSleep() {
  radio.sleep();
  digitalWrite(trigPin, LOW);

  DEBUG_PRINTLN("----------------------");
  Serial.flush();

  esp_sleep_enable_timer_wakeup(TRANSMIT_INTERVAL_US);
  esp_deep_sleep_start();
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);

  initializeRtcStateIfNeeded();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(batteryin, INPUT);

  digitalWrite(trigPin, LOW);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  analogSetPinAttenuation(batteryin, ADC_11db);

  if (!initializeRadio()) {
    // Sleep and retry instead of burning battery in a permanent error loop.
    enterDeepSleep();
  }
}

// -------- LOOP --------
void loop() {
  DEBUG_PRINTLN("Transmitting...");

  DistanceResult distance = readDistance();
  BatteryResult battery = readBattery();

  DEBUG_PRINT("Median distance: ");
  DEBUG_PRINTLN(distance.medianCm);
  DEBUG_PRINT("MAD: ");
  DEBUG_PRINTLN(distance.madCm);
  DEBUG_PRINT("Accepted samples: ");
  DEBUG_PRINTLN(distance.acceptedSamples);
  DEBUG_PRINT("Timeouts: ");
  DEBUG_PRINTLN(distance.timeouts);
  DEBUG_PRINT("Sensor confidence: ");
  DEBUG_PRINTLN(distance.confidence);
  DEBUG_PRINT("Sensor fault: ");
  DEBUG_PRINTLN(distance.sensorFault ? "yes" : "no");
  DEBUG_PRINT("Final distance: ");
  DEBUG_PRINTLN(distance.distanceCm);

  DEBUG_PRINT("ADC Raw: ");
  DEBUG_PRINTLN(battery.rawAdc);
  DEBUG_PRINT("ADC Voltage: ");
  if (DEBUG_SERIAL) Serial.println(battery.adcVoltage, 3);
  DEBUG_PRINT("Battery Voltage: ");
  if (DEBUG_SERIAL) Serial.println(battery.batteryVoltage, 2);
  DEBUG_PRINT("Battery %: ");
  DEBUG_PRINTLN(battery.percent);
  DEBUG_PRINT("Battery health: ");
  DEBUG_PRINTLN(battery.health);
  DEBUG_PRINT("Low battery: ");
  DEBUG_PRINTLN(battery.lowBattery ? "yes" : "no");

  // -------- Payload --------
  char payload[32];
  int written = snprintf(
    payload,
    sizeof(payload),
    "%d,%.2f,%d",
    distance.distanceCm,
    battery.batteryVoltage,
    battery.percent
  );

  if (written <= 0 || written >= (int)sizeof(payload)) {
    DEBUG_PRINTLN("Payload formatting failed");
    enterDeepSleep();
  }

  DEBUG_PRINT("Payload: ");
  DEBUG_PRINTLN(payload);

  int state = radio.transmit((uint8_t *)payload, strlen(payload));

  if (state == RADIOLIB_ERR_NONE) {
    DEBUG_PRINTLN("LoRa TX success!");
  } else {
    DEBUG_PRINT("failed, code ");
    DEBUG_PRINTLN(state);
  }

  enterDeepSleep();
}
