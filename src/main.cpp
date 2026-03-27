#include <RadioLib.h>

// NSS, DIO0, RESET, DIO1
SX1276 radio = new Module(5, 2, 14, 3);

int count = 0;

void setup() {
  Serial.begin(9600);

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

void loop() {
  Serial.print("Transmitting... ");

  String str = "Hello #" + String(count++);
  int state = radio.transmit(str);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("success!");
  } else {
    Serial.print("failed, code ");
    Serial.println(state);
  }

  delay(1000);
}