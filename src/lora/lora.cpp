// #include <Arduino.h>
// #include "lora.h"
// // #include "utils/config.h"

// SX1276 radio = new Module(
//     5,
//     2,
//     14,
//     34
// );

// void initLoRa() {
//     Serial.print("[SX1276] Initializing ... ");

//     int state = radio.begin(
//         865.0,
//         125.0,
//         9,
//         5,
//         0x12,
//         17,
//         8,
//         0
//     );

//     if (state == RADIOLIB_ERR_NONE) {
//         Serial.println("success!");
//     } else {
//         Serial.print("failed, code ");
//         Serial.println(state);
//         while (true);
//     }
// }

// LoRaPacket receiveLoRa() {
//     LoRaPacket pkt;
//     pkt.state = radio.receive(pkt.data);

//     if (pkt.state == RADIOLIB_ERR_NONE) {
//         Serial.println("Received: " + pkt.data);
//     }

//     return pkt;
// }