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
#define DIVIDER_RATIO  3.2

// -------- Battery Limits --------
#define BATTERY_EMPTY_V 6.4
#define BATTERY_FULL_V 8.4

// -------- Distance Limits --------
#define MAX_DISTANCE 120
#define MIN_DISTANCE 19          // sensor unreliable below this
#define MIN_VALID_DISTANCE 19    // clamp prediction here

// -------- Distance Filter Settings --------
#define RAW_SAMPLE_COUNT 9
#define MIN_SAMPLES_AFTER_MAD 3
#define ECHO_TIMEOUT_US 30000UL
#define SAMPLE_SPACING_MS 35
#define EMA_NUMERATOR 35
#define EMA_DENOMINATOR 100
#define MAX_FAILURES_BEFORE_STALE 6
#define UNRELIABLE_ENTER_CM 20
#define UNRELIABLE_EXIT_CM 22

// -------- Prediction Variables --------
int lastDistance = 0;
int lastRate = 0;
int lastValidDistance = 0;

// -------- Sensor Health State --------
int consecutiveFailures = 0;
int sensorConfidence = 0;
bool emaInitialized = false;
float emaDistance = 0.0;
bool unreliableRegionLatched = false;

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
int readDistanceRaw(bool *timedOut) {
  if (timedOut != NULL) {
    *timedOut = false;
  }

  digitalWrite(trigPin, LOW);
  delayMicroseconds(3);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Timeout prevents a bad echo cycle from blocking long-term operation.
  long duration = pulseIn(echoPin, HIGH, ECHO_TIMEOUT_US);

  if (duration == 0) {
    if (timedOut != NULL) {
      *timedOut = true;
    }
    return -1;
  }

  int dist = duration * 0.0343 / 2;

  // Reject impossible values before the statistical filters see them.
  if (dist < MIN_DISTANCE || dist > MAX_DISTANCE) return -1;

  return dist;
}

// -------- SORT HELPER --------
void sortReadings(int values[], int count) {
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

// -------- MEDIAN HELPER --------
int medianOfSorted(const int values[], int count) {
  if (count <= 0) return -1;

  if ((count % 2) == 0) {
    return (values[(count / 2) - 1] + values[count / 2]) / 2;
  }

  return values[count / 2];
}

// -------- ADAPTIVE OUTLIER LIMIT --------
int adaptiveOutlierLimit(int centre) {
  // Near a full tank the sensor is close to its unreliable zone, so the
  // acceptance band is deliberately tighter to reject false close echoes.
  if (centre <= 30) return 5;
  if (centre <= 60) return 7;
  if (centre <= 90) return 10;
  return 12;
}

// -------- RATE LIMIT --------
int rateLimitForDistance(int distance) {
  // Water level cannot jump instantly; allow slightly more movement when the
  // tank is emptier because geometry and refill/drain behaviour can vary.
  if (distance <= 30) return 3;
  if (distance <= 70) return 5;
  return 8;
}

// -------- UNRELIABLE REGION HYSTERESIS --------
int applyUnreliableRegionHysteresis(int value) {
  // The waterproof ultrasonic sensor is unreliable below 19 cm. Latch the
  // boundary through a small band so noise around the edge does not oscillate.
  if (unreliableRegionLatched) {
    if (value >= UNRELIABLE_EXIT_CM) {
      unreliableRegionLatched = false;
    } else {
      return MIN_VALID_DISTANCE;
    }
  } else if (value <= UNRELIABLE_ENTER_CM) {
    unreliableRegionLatched = true;
    return MIN_VALID_DISTANCE;
  }

  return value;
}

// -------- SENSOR HEALTH --------
void updateSensorHealth(int validRawCount, int acceptedCount, bool usedPrediction, bool totalLoss) {
  // Confidence is a compact health score for serial diagnostics only; the
  // payload format is intentionally unchanged.
  int confidence = 0;

  confidence += validRawCount * 8;
  confidence += acceptedCount * 5;

  if (usedPrediction) confidence -= 20;
  if (totalLoss) confidence -= 35;
  confidence -= consecutiveFailures * 8;

  if (confidence > 100) confidence = 100;
  if (confidence < 0) confidence = 0;

  sensorConfidence = confidence;
}

// -------- PREDICTION FILTER --------
int applyPrediction(int newValue, bool *usedPrediction) {
  if (usedPrediction != NULL) {
    *usedPrediction = false;
  }

  if (lastDistance == 0) {
    lastDistance = newValue;
    lastValidDistance = newValue;
    return newValue;
  }

  int currentRate = newValue - lastDistance;
  int smoothRate = (int)((lastRate * 0.7) + (currentRate * 0.3));

  // Clamp unrealistic speed using a level-aware physical movement limit.
  int maxRate = rateLimitForDistance(lastDistance);
  if (smoothRate > maxRate) smoothRate = maxRate;
  if (smoothRate < -maxRate) smoothRate = -maxRate;

  int predicted = lastDistance + smoothRate;

  // Clamp to minimum valid distance
  if (predicted < MIN_VALID_DISTANCE) predicted = MIN_VALID_DISTANCE;
  if (predicted > MAX_DISTANCE) predicted = MAX_DISTANCE;

  int diff = abs(newValue - predicted);
  int jumpLimit = adaptiveOutlierLimit(lastDistance) + maxRate;

  // Reject physically impossible jumps instead of trusting a single cycle.
  if (diff > jumpLimit) {
    Serial.println("Using predicted value; jump rejected");

    lastDistance = predicted;
    lastRate = smoothRate;
    if (usedPrediction != NULL) {
      *usedPrediction = true;
    }
    return predicted;
  }

  lastDistance = newValue;
  lastRate = smoothRate;
  lastValidDistance = newValue;

  return newValue;
}

// -------- TEMPORARY LOSS PREDICTION --------
int predictFromHistory() {
  // Gracefully degrade through short echo losses by carrying the recent trend.
  if (lastDistance == 0) {
    return MIN_VALID_DISTANCE;
  }

  int maxRate = rateLimitForDistance(lastDistance);
  int rate = lastRate;

  if (rate > maxRate) rate = maxRate;
  if (rate < -maxRate) rate = -maxRate;

  int predicted = lastDistance + rate;

  if (predicted < MIN_VALID_DISTANCE) predicted = MIN_VALID_DISTANCE;
  if (predicted > MAX_DISTANCE) predicted = MAX_DISTANCE;

  lastDistance = predicted;
  return predicted;
}

// -------- EXPONENTIAL MOVING AVERAGE --------
int applyEmaSmoothing(int value, bool usedPrediction) {
  // EMA smooths cycle-to-cycle noise while prediction cycles get a lighter
  // update so stale estimates do not drag the output too aggressively.
  int alpha = usedPrediction ? (EMA_NUMERATOR / 2) : EMA_NUMERATOR;

  if (!emaInitialized) {
    emaDistance = value;
    emaInitialized = true;
  } else {
    emaDistance = ((emaDistance * (EMA_DENOMINATOR - alpha)) + (value * alpha)) / EMA_DENOMINATOR;
  }

  int rounded = (int)(emaDistance + 0.5);

  if (rounded < MIN_VALID_DISTANCE) rounded = MIN_VALID_DISTANCE;
  if (rounded > MAX_DISTANCE) rounded = MAX_DISTANCE;

  return rounded;
}

// -------- STABLE DISTANCE --------
int readDistance() {
  int readings[RAW_SAMPLE_COUNT];
  int deviations[RAW_SAMPLE_COUNT];
  int count = 0;
  int timeouts = 0;

  for (int i = 0; i < RAW_SAMPLE_COUNT; i++) {
    bool timedOut = false;
    int d = readDistanceRaw(&timedOut);
    Serial.print("Raw reading: ");
    Serial.println(d);

    if (timedOut) {
      timeouts++;
    }

    if (d != -1) {
      readings[count++] = d;
    }

    delay(SAMPLE_SPACING_MS);
  }

  // -------- No echo -> predict --------
  //
  if (count == 0) {
    consecutiveFailures++;
    int predicted = predictFromHistory();
    bool usedPrediction = true;
    int stable = applyEmaSmoothing(predicted, usedPrediction);
    stable = applyUnreliableRegionHysteresis(stable);

    // After repeated losses, hold the last stable value instead of suddenly
    // reporting the unreliable-zone boundary.
    if (consecutiveFailures > MAX_FAILURES_BEFORE_STALE && lastValidDistance > 0) {
      stable = lastValidDistance;
    }

    updateSensorHealth(0, 0, usedPrediction, true);

    Serial.println("No echo; using graceful prediction");
    Serial.print("Sensor confidence: ");
    Serial.println(sensorConfidence);
    Serial.print("Final distance: ");
    Serial.println(stable);
    return stable;
  }

  // -------- Sort for median --------
  sortReadings(readings, count);
  int median = medianOfSorted(readings, count);

  // -------- MAD outlier rejection --------
  for (int i = 0; i < count; i++) {
    deviations[i] = abs(readings[i] - median);
  }

  sortReadings(deviations, count);
  int mad = medianOfSorted(deviations, count);
  int outlierLimit = adaptiveOutlierLimit(median);
  int madLimit = mad * 3;

  if (madLimit < outlierLimit) {
    madLimit = outlierLimit;
  }

  long sum = 0;
  int accepted = 0;

  for (int i = 0; i < count; i++) {
    if (abs(readings[i] - median) <= madLimit) {
      sum += readings[i];
      accepted++;
    }
  }

  // MAD can collapse to zero when most echoes agree; fall back to the median
  // if the remaining set is too small for a trustworthy average.
  int averaged = median;
  if (accepted >= MIN_SAMPLES_AFTER_MAD) {
    averaged = (int)((sum + (accepted / 2)) / accepted);
  }

  // A sparse or heavily rejected cycle is degraded even when it has a median.
  // Track it so repeated weak cycles lower confidence and favour prediction.
  bool weakCycle = (count < MIN_SAMPLES_AFTER_MAD) || (accepted < MIN_SAMPLES_AFTER_MAD);
  if (weakCycle) {
    consecutiveFailures++;
  } else {
    consecutiveFailures = 0;
  }

  bool usedPrediction = false;
  int predictedValue = applyPrediction(averaged, &usedPrediction);
  int stable = applyEmaSmoothing(predictedValue, usedPrediction);
  int finalValue = applyUnreliableRegionHysteresis(stable);

  lastValidDistance = finalValue;
  updateSensorHealth(count, accepted, usedPrediction, false);

  Serial.print("Median distance: ");
  Serial.println(median);
  Serial.print("MAD: ");
  Serial.println(mad);
  Serial.print("Accepted samples: ");
  Serial.println(accepted);
  Serial.print("Timeouts: ");
  Serial.println(timeouts);
  Serial.print("Sensor confidence: ");
  Serial.println(sensorConfidence);
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
  float percent = ((voltage - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V)) * 100.0;

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
