# Heltec Gateway V7 | Recovery Firmware | 2026-10-20

Production source: [Heltec-Gateway-v7](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v7).
Safe rollback: [Heltec-Gateway-v6](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v6).

Target: **Heltec WiFi LoRa 32 V4 OLED**, ESP32-S3, PlatformIO \`heltec-v4\`.

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
