# V5.1 HOBO recovery and standard telemetry transport

## What this changes

This patch addresses two different failure layers that had been getting mixed together:

1. A RAK can keep scanning or passing battery telemetry while its nRF52 central/GATT path no longer obtains a real HOBO `STATUS` response.
2. Automatic MX2001 data used a custom `PRIVATE_APP` payload while ordinary battery telemetry used Meshtastic `TELEMETRY_APP`; a gateway could receive the latter normally while missing or filtering the former.

The implementation does not claim a single physical cause for every failed connection. `rsp=0 gatt=FFFF n=0` remains a diagnostic meaning that service discovery saw no response, not a decoded `0xFFFF` GATT error. Strong BLE RSSI at Mill Creek rules out simple range as the universal explanation. USB/button events can help by resetting or keeping the local nRF52 central state awake, but they do not prove the logger itself was always responsive.

## Recovery policy

- A decoded HOBO `STATUS` reply is the only health proof. Scanner state, advertisements, LoRa traffic, and battery packets do not clear the health timer.
- Missing-loggers receive an active 50%-duty acquisition scan for two minutes at boot, after disconnect/recovery, and every five minutes while missing. The normal state remains the original 10%-duty passive scan. This is deliberately bounded so healthy stations do not pay the battery cost.
- The RAK handshake cooldown is 30 seconds rather than five minutes: enough to prevent tight retry churn, but not a five-minute blind period after an otherwise recoverable GATT failure.
- After 15 minutes without `STATUS`, the module disconnects/cancels/restarts the central link without changing logger lock, calibration, mesh keys, or node identity.
- If no `STATUS` arrives five minutes after that recovery, it arms one watchdog-origin whole-node reset. This rebuilds the nRF52 BLE/SoftDevice and radio stack without erasing settings.
- A retained `.noinit` guard allows only one such health-triggered whole-node reset until a real `STATUS` returns. A dead logger therefore cannot reboot a working relay every few minutes.
- The existing 12-hour whole-node maintenance reset remains, now staggered by node ID over ten minutes so relays do not restart together.

## Commands

- `HOBO RESTART` — queue a BLE-central reset only.
- `BLE DIAG` or `HOBO STATUS` — show link/scanner state, last verified STATUS age, recovery count, and whether the one whole-node reset is available or already used.
- `BLE RESET STATUS` — show the saved automatic recovery interval and health state.
- `BLE RESET AUTO 120` — save a 15–720 minute repeat interval for BLE-only stale-STATUS recovery. The default is 120 minutes.
- `BLE RESET AUTO OFF` — disable automatic stale-STATUS recovery. The 12-hour maintenance reboot remains.

## Standard telemetry form

MX2001 automatic measurements now send normal `TELEMETRY_APP` `EnvironmentMetrics`:

| Field | Value |
| --- | --- |
| `temperature` | HOBO temperature in °C |
| `distance` | HOBO calibrated water stage in mm (`stage_ft × 304.8`) |

Battery/device metrics and environment metrics are separate protobuf oneof messages, but both now use the same standard telemetry port and gateway path. The companion Heltec gateway patch recognizes the MX2001 temperature-plus-distance form only from known MX2001 stations and serializes it as an `mx2001` ingestion record.

Automatic `PRIVATE_APP` MX packets are removed by design; manual `READ` replies retain MAC/BLE RSSI for diagnostics.
