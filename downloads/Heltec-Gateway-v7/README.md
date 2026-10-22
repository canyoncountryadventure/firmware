# Heltec Gateway V7 | Recovery + Adaptive Polling Firmware | 2026-10-22

Production source: [Heltec-Gateway-v7](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v7).
Safe rollback: [Heltec-Gateway-v6](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v6).

Target: **Heltec WiFi LoRa 32 V4 OLED**, ESP32-S3, PlatformIO \`heltec-v4\`.

Verified application image: `Heltec-Gateway-v7-10-22-26.bin`  
Source commit: `076dd4d0d36f20af06dc3ffa4a89662ff9e7ceec`  
SHA-256: `6960c143d8c8c1a78ad37e2d75368a4590d93873aa1a62f54382436a66a1504e`

## What changed from V6
- The pre-existing 90-second task watchdog and scheduled 12-hour main-loop reboot remain.
- The main-loop recovery supervisor detects when the HTTP gateway worker makes no progress for 2 minutes.
- Counts real radio RX and **TX_DONE interrupt completion**, not just a queued DM or the old \`txGood\` metric.
- After 60 minutes with neither RX nor completed TX, provided DM requests were attempted, resets the Heltec.
- With TX still working but no RX, requests a guarded SX126x hardware reinitialize after 90 minutes, then a full restart if no RX after 3 hours.
- If the upload queue has been unable to attempt POST for 10 minutes while Wi-Fi is connected, or HTTPS transport failures persist for 60 minutes, resets the gateway.
- When Wi-Fi stays disconnected for 60 minutes, requests one software recovery restart; the Meshtastic Wi-Fi reconnect mechanism still runs normally.
- Continues CC01 \`WATER READ\` / \`READ\` RF polling while Wi-Fi is disconnected. No changes to DM reply parsing, automatic telemetry, sensor calibration, or node IDs.
- Recovery reboots are limited to **two consecutive attempts** until the device demonstrates successful RX and cloud upload and one hour of uptime.
- Adds supervisor ages/reason/counters to \`GET http://192.168.1.147/json/gateway\`.

## Installation
Only use the dated **non-factory** \`Heltec-Gateway-v7-MM-DD-YY.bin\` for Unified OTA; the ZIP contains the other supported flash tools. **Do not erase Meshtastic NVS/settings and do not flash a factory image** for normal Wi-Fi OTA. Check \`BUILD.txt\` for the source commit and SHA-256 before flashing. No deployment or flash happens automatically.

Build workflow: [Build Heltec Gateway V7](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_cca_heltec_gateway_v7.yml).
Dated images are published into [this directory](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v7/downloads/Heltec-Gateway-v7) only after the build and compatibility tests pass.

## Diagnostics and limitations
The supervisor uses the existing main task, protected by Meshtastic's hardware-backed task watchdog. It is not an external independent hardware watchdog and cannot recover every power/SoC lockup.

Reason IDs: 1=worker stalled, 2=no RX or TX, 4=no RX for 3 hours, 5=upload stalled, 6=Wi-Fi stalled. The radio soft-recovery reason is 3 but does not itself restart the board.

Radio silence alone cannot prove the gateway receiver is faulty: nodes, relays, propagation, or antennas may also be offline. Excess restarts can lose the unsent RAM queue. A non-2xx HTTP rejection such as 401 or 500 is **not** interpreted as a gateway transport freeze.

## October 22 V7 polling, controls, and diagnostics update

This update **modifies V7 without changing its reboot and supervisor behavior**. On boot, the first CC01 cycle begins after about 2 minutes. Subsequent scheduled cycles begin hourly when the last validated CC01 ultrasonic stage is at or below 0.75 ft, or every 15 minutes after stage rises above 0.75 ft. Fast flood mode persists across gateway reboots and remains in effect until a validated lower stage arrives. This is a Heltec DM *polling* interval change; it does not alter the HOBO's own recording interval or the RAK's existing automatic telemetry interval.

Each enabled cycle runs `WATER READ` first, then `READ`. Each command makes up to three total requests, 30 seconds apart. A successful measurement stops that command's requests. Explicit `HOBO unavailable` stops that sensor immediately without wasting the remaining two requests. A valid reply may still upload regardless of polling phase; stale/out-of-order replies do not change the current phase. Missing messages are not confused with proven sensor failures. Existing automatic packets continue flowing to Neon even when one or both Heltec DMs are paused.

Cloud controls: owner administration at https://seutahwatersheds.vercel.app/notifications stores desired `WATER READ` and `HOBO READ` states in Neon. Heltec retrieves changes over authenticated HTTPS roughly every 2 minutes and reports its actual applied state. Pauses persist across reboots in nonvolatile storage. The authorized DM radio node ID is configured from the same protected page (0 means no remote radio control); `PAUSE HOBO`, `RESUME HOBO`, `PAUSE SENSOR`, `RESUME SENSOR`, `POLL STATUS` affect only Heltec-requested commands. Web updates have precedence when configuration version advances; offline DM overrides are reported as actual state so web and radio disagreements are visible.

Errors: CC01 `HOBO unavailable` posts a `hobo/ble_unavailable` diagnostic and `READ failed: TIMEOUT` in a pending ultrasonic request posts `water/timeout`. These are stored separately from actual water level and temperature and displayed as `BLE to HOBO unavailable` and `Needs maintenance` on the respective dashboard sensor cards. Valid subsequent sensor telemetry clears a displayed failure. Pausing polling suppresses the corresponding active warning while paused. Database migration: `canyoncountryadventure/Meshtastic/migrations/20261022_cc01_gateway_control.sql`. The cloud control URL is the existing `/api/ingest?control=cc01`, authenticated with the current gateway ingest credential, so no extra Vercel function is required.

The previous 12-hour preventive reboot, 90-second task watchdog, V7 gateway health supervisor, radio maintenance, HTTPS upload retries, station identity, calibration, existing auto packets, mesh channel and logger lock are not modified. **Do not flash an unverified date-specific image**; the GitHub Actions build must pass and publish the matching BIN/ZIP/BUILD.txt first.
