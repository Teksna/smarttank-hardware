#include <RadioLib.h>

// NSS, DIO0, RESET, DIO1
SX1276 radio = new Module(5, 2, 14, 3);

void setup() {
  Serial.begin(9600);

  Serial.print("[SX1276] Initializing ... ");

  int state = radio.begin(
    865.0,   // frequency
    125.0,   // bandwidth (must match TX)
    9,       // spreading factor
    5,       // coding rate
    0x12,    // sync word
    17,      // power
    8,       // preamble length
    0        // gain (auto)
  );

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("success!");
  } else {
    Serial.print("failed, code ");
    Serial.println(state);
    while (true);
  }
}

void loop() {
  Serial.print("Waiting... ");

  String str;

  int state = radio.receive(str);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("Received!");

    Serial.print("Data: ");
    Serial.println(str);

    Serial.print("RSSI: ");
    Serial.print(radio.getRSSI());
    Serial.println(" dBm");

    Serial.print("SNR: ");
    Serial.print(radio.getSNR());
    Serial.println(" dB");

    Serial.print("Freq Error: ");
    Serial.print(radio.getFrequencyError());
    Serial.println(" Hz");

  } 
  else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.println("timeout");
  } 
  else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println("CRC error");
  } 
  else {
    Serial.print("failed, code ");
    Serial.println(state);
  }
}