# Changelog

## 2026-07-09 - Asynchronous LoRa Receiver Rewrite

### LoRa Reception
- Replaced blocking `radio.receive()` with interrupt-driven receive flow.
- Added `radio.startReceive()` so the SX1276 remains in RX mode between packets.
- Added `radio.setPacketReceivedAction(onLoRaDio0)` for the installed RadioLib version.
- Kept the ISR short and interrupt-safe: it only sets a packet flag and increments a counter.
- Moved packet parsing, RSSI reading, tank calculations, motor decisions, display updates, WiFi, OTA, and Supabase calls out of the interrupt path.
- Restarted LoRa receive immediately after `readData()` so post-processing does not keep the radio idle.
- Replaced Arduino `String` packet parsing with a fixed 64-byte packet buffer and `strtol`/`strtof`.

### Main Loop Architecture
- Reworked `loop()` into cooperative scheduled handlers:
  - `handleLoRa()`
  - `handleWiFi()`
  - `handleMotor()`
  - `handleCloud()`
  - `handleOTA()`
  - `handleDisplay()`
  - `handleDiagnostics()`
- Removed multi-second application delays from the main packet path.
- Added `yield()` in the main loop to keep the ESP32 scheduler and watchdog healthy.

### WiFi
- Replaced blocking WiFi connection loops with a timed reconnect state machine.
- Added reconnect throttling so WiFi does not retry every loop iteration.
- Preserved the two configured WiFi networks and alternates between them when connection attempts fail.

### Supabase / Cloud
- Moved Supabase RPC execution into a timed cloud handler.
- Cloud sync now only runs when fresh packet data is available and the upload interval has elapsed.
- LoRa packet reception no longer waits for Supabase before returning to RX mode.
- Preserved the existing RPC API and cloud response fields.

### OTA
- Removed the startup OTA check from the blocking setup path.
- Added periodic OTA checks through the scheduler.
- OTA checks only run when WiFi is connected.
- Preserved the existing `checkForOTAUpdate()` implementation and behavior.

### Motor Control
- Preserved the existing automation flags, supply gating, tank refill tracking, overflow threshold behavior, and hysteresis-style decision state.
- Replaced relay-control delays with immediate GPIO state application.
- Relay GPIO is only written when the desired motor state changes.
- Motor evaluation now runs periodically instead of only after blocking packet/cloud/display work.

### Tank Level Processing
- Preserved the packet format: `distance,battery_voltage,battery_percentage`.
- Preserved the existing full-distance calibration and stable-level filtering behavior.
- Kept the anti-freeze fallback for unstable level updates.
- Continued applying the cloud overflow threshold to the empty-distance calculation, matching existing behavior.

### Display
- Removed the blocking `delay(2000)` from `displayStatus()`.
- Display redraws are now dirty-flagged and rate-limited from the scheduler.
- Restored the previous motor-status-then-water-status screen sequence without blocking delays.
- Prevented display overlap by drawing motor status and water tank status as separate full-screen views.
- Fixed RSSI icon coloring to use the actual received RSSI instead of a hardcoded `-70`.
- Fixed battery icon fill to use the actual battery percentage instead of a hardcoded `82`.
- Fixed motor icon state so OFF renders as the OFF style.

### Diagnostics
- Added optional task timing instrumentation behind `DEBUG_TIMING`.
- When enabled, timing captures LoRa, WiFi, cloud, OTA, motor, display, interrupt count, and free heap.

### Reliability Improvements
- Removed blocking LoRa receive from the application loop.
- Removed repeated WiFi reconnect attempts from every loop cycle.
- Reduced heap churn from LoRa packet parsing.
- Reduced watchdog risk by removing multi-second delays from normal packet processing.
- Reduced duplicate relay writes and unnecessary display refreshes.

### Notes
- The installed RadioLib version exposes `setPacketReceivedAction()` as the preferred packet-received callback API. `setDio0Action()` requires an additional interrupt edge argument in this version.
- PlatformIO compilation was not run after this change because command approval was denied.
