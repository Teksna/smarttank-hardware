#include <Arduino.h>
#include <WiFi.h>
#include <RadioLib.h>

#include "display/display.h"
#include "ota/ota.h"
#include "supabase/supabase.h"

// ---------------- BUILD OPTIONS ----------------
#ifndef DEBUG_TIMING
#define DEBUG_TIMING 0
#endif

// ---------------- WIFI ----------------
static const char* WIFI_SSID_1 = "Airtel_mohd_3792";
static const char* WIFI_PASS_1 = "Air@28347";
static const char* WIFI_SSID_2 = "Airtel_umai_0321";
static const char* WIFI_PASS_2 = "air96843";

// ---------------- SUPABASE ----------------
static const char* SUPABASE_URL = "https://yljggigahlagdihhycfj.supabase.co";
static const char* SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InlsamdnaWdhaGxhZ2RpaGh5Y2ZqIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzI3MDcwNTMsImV4cCI6MjA4ODI4MzA1M30.NeGRlQv-T-OGW4iqJPLV2T-2uPQxDNz0r9GHLAG8F-g";

// ---------------- LORA ----------------
static constexpr int LORA_SS = 5;
static constexpr int LORA_RST = 14;
static constexpr int LORA_DIO0 = 2;
static constexpr int LORA_DIO1 = 34;
static constexpr size_t LORA_PACKET_BUFFER_SIZE = 64;

static SX1276 radio = new Module(LORA_SS, LORA_DIO0, LORA_RST, LORA_DIO1);
static volatile bool loraPacketAvailable = false;
static volatile uint32_t loraInterruptCount = 0;

// ---------------- MOTOR ----------------
static constexpr int MOTOR_PIN_NO = 27;
static constexpr int MOTOR_PIN_NC = 26;
static bool motorState = false;
static bool motorDecisionState = false;
static bool tankJustFilled = true;
static bool relayWritePending = true;

// ---------------- TANK ----------------
static constexpr int FULL_DISTANCE_CM = 19;
static int emptyDistanceCm = 120;
static int overflowThreshold = 90;
static int previousLevel = 50;
static int lastStableLevel = 0;
static int stableCount = 0;
static int noUpdateCount = 0;
static bool levelInitialized = false;

// ---------------- RECEIVER STATE ----------------
struct ReceiverState {
    bool hasPacket = false;
    bool displayDirty = true;
    bool cloudDirty = false;
    int distance = -1;
    int tankLevel = 0;
    int rssi = 0;
    float batteryVoltage = 0.0f;
    int batteryPercent = 0;
};

static ReceiverState receiver;
static bool supplyState = false;
static bool motorAutomationState = false;

// ---------------- SCHEDULER ----------------
static constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 15000;
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 6000;
static constexpr uint32_t CLOUD_UPLOAD_INTERVAL_MS = 3000;
static constexpr uint32_t OTA_CHECK_INTERVAL_MS = 15UL * 60UL * 1000UL;
static constexpr uint32_t MOTOR_EVALUATE_INTERVAL_MS = 250;
static constexpr uint32_t DISPLAY_MIN_INTERVAL_MS = 500;
static constexpr uint32_t DISPLAY_MOTOR_SCREEN_MS = 2000;
static constexpr uint32_t DIAGNOSTIC_INTERVAL_MS = 10000;

static uint32_t lastWiFiAttemptMs = 0;
static uint32_t wifiAttemptStartedMs = 0;
static uint8_t wifiCredentialIndex = 0;
static bool wifiConnectInProgress = false;
static uint32_t lastCloudUploadMs = 0;
static uint32_t lastOtaCheckMs = 0;
static uint32_t lastMotorEvaluateMs = 0;
static uint32_t lastDisplayMs = 0;
static uint32_t lastDiagnosticMs = 0;
static uint32_t motorScreenUntilMs = 0;
static bool motorScreenActive = false;

struct TaskTiming {
    uint32_t lastUs = 0;
    uint32_t maxUs = 0;
};

static TaskTiming loraTiming;
static TaskTiming wifiTiming;
static TaskTiming cloudTiming;
static TaskTiming otaTiming;
static TaskTiming motorTiming;
static TaskTiming displayTiming;

static inline void recordTiming(TaskTiming& timing, uint32_t startedUs) {
#if DEBUG_TIMING
    timing.lastUs = micros() - startedUs;
    if (timing.lastUs > timing.maxUs) {
        timing.maxUs = timing.lastUs;
    }
#else
    (void)timing;
    (void)startedUs;
#endif
}

static void IRAM_ATTR onLoRaDio0() {
    loraPacketAvailable = true;
    loraInterruptCount++;
}

static int calculateWaterLevel(int distance) {
    if (distance <= FULL_DISTANCE_CM) {
        return 100;
    }
    if (distance >= emptyDistanceCm) {
        return 0;
    }

    const float percent = static_cast<float>(emptyDistanceCm - distance) * 100.0f /
                          static_cast<float>(emptyDistanceCm - FULL_DISTANCE_CM);
    return static_cast<int>(percent + 0.5f);
}

static bool parsePacket(const char* packet, int& distance, float& batteryVoltage, int& batteryPercent) {
    char* endPtr = nullptr;
    const long parsedDistance = strtol(packet, &endPtr, 10);
    if (endPtr == packet || *endPtr != ',') {
        return false;
    }

    const float parsedVoltage = strtof(endPtr + 1, &endPtr);
    if (*endPtr != ',') {
        return false;
    }

    const long parsedBattery = strtol(endPtr + 1, &endPtr, 10);
    if (*endPtr != '\0' && *endPtr != '\r' && *endPtr != '\n') {
        return false;
    }

    distance = static_cast<int>(parsedDistance);
    batteryVoltage = parsedVoltage;
    batteryPercent = constrain(static_cast<int>(parsedBattery), 0, 100);
    return true;
}

static void updateStableLevel(int waterLevel) {
    if (!levelInitialized) {
        previousLevel = waterLevel;
        lastStableLevel = waterLevel;
        levelInitialized = true;
        Serial.println("Initial level set");
    }

    if (abs(waterLevel - previousLevel) < 30) {
        stableCount++;
    } else {
        stableCount = 0;
    }

    if (stableCount >= 1) {
        previousLevel = waterLevel;
        lastStableLevel = waterLevel;
        noUpdateCount = 0;
        return;
    }

    noUpdateCount++;
    if (noUpdateCount > 3) {
        Serial.println("Force update after unstable readings");
        lastStableLevel = waterLevel;
        previousLevel = waterLevel;
        noUpdateCount = 0;
    }
}

static void beginWiFiConnection() {
    const char* ssid = (wifiCredentialIndex == 0) ? WIFI_SSID_1 : WIFI_SSID_2;
    const char* pass = (wifiCredentialIndex == 0) ? WIFI_PASS_1 : WIFI_PASS_2;

    Serial.print("Connecting WiFi: ");
    Serial.println(ssid);
    WiFi.disconnect(false, false);
    WiFi.begin(ssid, pass);
    wifiAttemptStartedMs = millis();
    lastWiFiAttemptMs = wifiAttemptStartedMs;
    wifiConnectInProgress = true;
}

static void handleWiFi() {
    const uint32_t startedUs = micros();
    const uint32_t now = millis();

    if (WiFi.status() == WL_CONNECTED) {
        if (wifiConnectInProgress) {
            Serial.print("WiFi connected, IP: ");
            Serial.println(WiFi.localIP());
        }
        wifiConnectInProgress = false;
        recordTiming(wifiTiming, startedUs);
        return;
    }

    if (wifiConnectInProgress && now - wifiAttemptStartedMs < WIFI_CONNECT_TIMEOUT_MS) {
        recordTiming(wifiTiming, startedUs);
        return;
    }

    if (!wifiConnectInProgress || now - lastWiFiAttemptMs >= WIFI_RECONNECT_INTERVAL_MS) {
        wifiCredentialIndex = (wifiConnectInProgress && wifiCredentialIndex == 0) ? 1 : 0;
        beginWiFiConnection();
    }

    recordTiming(wifiTiming, startedUs);
}

static void initMotorPins() {
    pinMode(MOTOR_PIN_NO, OUTPUT);
    pinMode(MOTOR_PIN_NC, OUTPUT);
    digitalWrite(MOTOR_PIN_NC, LOW);
    digitalWrite(MOTOR_PIN_NO, LOW);
    motorState = false;
    Serial.println("Motor pins initialized");
}

static void applyMotorOutput(bool shouldRunMotor) {
    if (!relayWritePending && motorState == shouldRunMotor) {
        return;
    }

    digitalWrite(MOTOR_PIN_NO, shouldRunMotor ? HIGH : LOW);
    motorState = shouldRunMotor;
    relayWritePending = false;
    receiver.displayDirty = true;
    motorScreenActive = true;
    motorScreenUntilMs = 0;

    Serial.print("Motor ");
    Serial.println(motorState ? "ON" : "OFF");
}

static void handleMotor() {
    const uint32_t startedUs = micros();
    const uint32_t now = millis();
    if (now - lastMotorEvaluateMs < MOTOR_EVALUATE_INTERVAL_MS) {
        recordTiming(motorTiming, startedUs);
        return;
    }
    lastMotorEvaluateMs = now;

    if (!receiver.hasPacket) {
        applyMotorOutput(false);
        recordTiming(motorTiming, startedUs);
        return;
    }

    const int controlLevel = lastStableLevel;
    const int upperBound = constrain(overflowThreshold, 0, 100);

    if (motorAutomationState && supplyState) {
        if (controlLevel < 95 && !motorDecisionState && tankJustFilled) {
            tankJustFilled = false;
        }

        if (controlLevel < 99 && !motorDecisionState && !tankJustFilled) {
            motorDecisionState = true;
        } else if (controlLevel >= upperBound) {
            motorDecisionState = false;
            tankJustFilled = true;
        }
    } else {
        motorDecisionState = false;
    }

    applyMotorOutput(motorDecisionState);
    recordTiming(motorTiming, startedUs);
}

static void handleLoRa() {
    const uint32_t startedUs = micros();
    if (!loraPacketAvailable) {
        recordTiming(loraTiming, startedUs);
        return;
    }

    noInterrupts();
    loraPacketAvailable = false;
    interrupts();

    uint8_t packetBytes[LORA_PACKET_BUFFER_SIZE] = {0};
    size_t packetLength = radio.getPacketLength();
    if (packetLength >= LORA_PACKET_BUFFER_SIZE) {
        packetLength = LORA_PACKET_BUFFER_SIZE - 1;
    }

    const int state = radio.readData(packetBytes, packetLength);
    radio.startReceive();

    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("LoRa read failed, code ");
        Serial.println(state);
        recordTiming(loraTiming, startedUs);
        return;
    }

    packetBytes[packetLength] = '\0';

    int distance = -1;
    float batteryVoltage = 0.0f;
    int batteryPercent = 0;
    if (!parsePacket(reinterpret_cast<const char*>(packetBytes), distance, batteryVoltage, batteryPercent)) {
        Serial.print("Invalid packet: ");
        Serial.println(reinterpret_cast<const char*>(packetBytes));
        recordTiming(loraTiming, startedUs);
        return;
    }

    const int waterLevel = calculateWaterLevel(distance);
    updateStableLevel(waterLevel);

    receiver.hasPacket = true;
    receiver.distance = distance;
    receiver.tankLevel = lastStableLevel;
    receiver.rssi = static_cast<int>(radio.getRSSI());
    receiver.batteryVoltage = batteryVoltage;
    receiver.batteryPercent = batteryPercent;
    receiver.displayDirty = true;
    receiver.cloudDirty = true;
    motorScreenActive = true;
    motorScreenUntilMs = 0;

    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.print(" cm | Water Level: ");
    Serial.print(receiver.tankLevel);
    Serial.print("% | RSSI: ");
    Serial.println(receiver.rssi);

    recordTiming(loraTiming, startedUs);
}

static void handleCloud() {
    const uint32_t startedUs = micros();
    const uint32_t now = millis();
    if (!receiver.hasPacket || !receiver.cloudDirty || now - lastCloudUploadMs < CLOUD_UPLOAD_INTERVAL_MS) {
        recordTiming(cloudTiming, startedUs);
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        recordTiming(cloudTiming, startedUs);
        return;
    }

    CloudResponse cloud = ota_device_update_and_fetch(
        receiver.tankLevel,
        motorState,
        receiver.rssi,
        receiver.batteryPercent
    );

    supplyState = cloud.supply;
    motorAutomationState = cloud.motorAutomation;
    overflowThreshold = (cloud.overflowThreshold > 0) ? cloud.overflowThreshold : 90;
    emptyDistanceCm = overflowThreshold;
    receiver.cloudDirty = false;
    lastCloudUploadMs = now;

    if (cloud.ota) {
        Serial.println("Cloud requested reboot for OTA");
        ESP.restart();
    }

    recordTiming(cloudTiming, startedUs);
}

static void handleOTA() {
    const uint32_t startedUs = micros();
    const uint32_t now = millis();
    if (now - lastOtaCheckMs < OTA_CHECK_INTERVAL_MS) {
        recordTiming(otaTiming, startedUs);
        return;
    }
    lastOtaCheckMs = now;

    if (WiFi.status() == WL_CONNECTED) {
        checkForOTAUpdate();
    }

    recordTiming(otaTiming, startedUs);
}

static void handleDisplay() {
    const uint32_t startedUs = micros();
    const uint32_t now = millis();

    if (now - lastDisplayMs < DISPLAY_MIN_INTERVAL_MS) {
        recordTiming(displayTiming, startedUs);
        return;
    }

    if (motorScreenActive) {
        if (motorScreenUntilMs == 0) {
            displayMotorStatus(motorState);
            motorScreenUntilMs = now + DISPLAY_MOTOR_SCREEN_MS;
            lastDisplayMs = now;
            recordTiming(displayTiming, startedUs);
            return;
        }

        if (static_cast<int32_t>(now - motorScreenUntilMs) < 0) {
            recordTiming(displayTiming, startedUs);
            return;
        }

        motorScreenActive = false;
        motorScreenUntilMs = 0;
        receiver.displayDirty = true;
    }

    if (receiver.displayDirty) {
        displayStatus(receiver.tankLevel, receiver.rssi, receiver.batteryPercent);
        receiver.displayDirty = false;
        lastDisplayMs = now;
    }

    recordTiming(displayTiming, startedUs);
}

static void handleDiagnostics() {
#if DEBUG_TIMING
    const uint32_t now = millis();
    if (now - lastDiagnosticMs < DIAGNOSTIC_INTERVAL_MS) {
        return;
    }
    lastDiagnosticMs = now;

    Serial.printf(
        "Timing us last/max - LoRa:%lu/%lu WiFi:%lu/%lu Cloud:%lu/%lu OTA:%lu/%lu Motor:%lu/%lu Display:%lu/%lu IRQ:%lu Heap:%lu\n",
        loraTiming.lastUs, loraTiming.maxUs,
        wifiTiming.lastUs, wifiTiming.maxUs,
        cloudTiming.lastUs, cloudTiming.maxUs,
        otaTiming.lastUs, otaTiming.maxUs,
        motorTiming.lastUs, motorTiming.maxUs,
        displayTiming.lastUs, displayTiming.maxUs,
        loraInterruptCount,
        ESP.getFreeHeap()
    );
#endif
}

static void initLoRa() {
    Serial.print("[SX1276] Initializing ... ");
    const int state = radio.begin(865.0, 125.0, 9, 5, 0x12, 17, 8, 0);
    if (state != RADIOLIB_ERR_NONE) {
        Serial.print("failed, code ");
        Serial.println(state);
        while (true) {
            delay(1000);
        }
    }

    radio.setPacketReceivedAction(onLoRaDio0);
    const int receiveState = radio.startReceive();
    Serial.println(receiveState == RADIOLIB_ERR_NONE ? "success!" : "RX start failed");
}

void setup() {
    Serial.begin(115200);
    initDisplay();
    initMotorPins();

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    beginWiFiConnection();

    supabaseInit(SUPABASE_URL, SUPABASE_KEY);
    initLoRa();

    lastOtaCheckMs = millis();
}

void loop() {
    handleLoRa();
    handleWiFi();
    handleMotor();
    handleCloud();
    handleOTA();
    handleDisplay();
    handleDiagnostics();
    yield();
}
