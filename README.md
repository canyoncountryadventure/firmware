# CCES Heltec Gateway V12 | Reset Diagnosis and Auto-DM Reporting

**Candidate firmware • November 26, 2026 • Heltec WiFi LoRa 32 V4 / ESP32-S3 / PlatformIO `heltec-v4`**

V12 extends [field-booted V11](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v11) with two observability features. It is **not** installed by publishing this branch; an operator must separately approve an exact CI-verified release through Cloudflare.

## Why V12 exists

V11 successfully installed remotely and reached OTA phase 4, but Cloudflare subsequently received health messages at approximately 15-minute intervals with **only ~30 seconds of uptime each**. Because `uptime_seconds` derives from ESP32 `millis()`, those are **application/device restarts, not merely HTTP reconnects**. We do not yet know whether resets are due to a software reboot, watchdog, crash, brownout or power loss.

V11 already records its last ESP32 reset reason in the `heltecdiag` subsystem but only surfaces it locally. V12 adds remote authenticated transmission of that **existing reset evidence**, without disabling any recovery or changing reboot policies.

## Download and build

- [V12 GitHub Actions workflow](.github/workflows/build_cca_heltec_gateway_v12.yml)
- [V12 dated release directory](downloads/Heltec-Gateway-v12/README.md)
- [Expected V12 application BIN](downloads/Heltec-Gateway-v12/Heltec-Gateway-v12-11-26-26.bin) **(usable only when CI successfully publishes it)**
- [V12 ZIP](downloads/Heltec-Gateway-v12/Heltec-Gateway-v12-11-26-26.zip) **(only after publication)**
- [V12 `BUILD.txt` for exact SHA-256 and compiled source](downloads/Heltec-Gateway-v12/BUILD.txt) **(must exist before authorizing)**
- [V11 field-confirmed binary and checksum](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v11/downloads/Heltec-Gateway-v11)
- [V9 Clock, last firmware before Cloudflare/remote integration](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary)

**Do not download by guessed filename.** The CI run must be green and the published release `BUILD.txt` must exist and identify the intended source SHA/firmware BIN hash. Firmware is application-only; never factory erase or rewrite the independent OTA_1 loader.

## V12 changes

| Function | V11 | V12 |
| --- | --- | --- |
| Cloudflare health | Firmware, uptime, queue, config, OTA state | Also `reset_reason_code` and `reset_diagnostic`, derived from existing `esp_reset_reason()` and retained `esp32FieldDiagFormat()` |
| Auto-DM visibility | Local-only `auto_fallback.watched` and saved NVS settings | Also authenticated `auto_dm_state` snapshots of **actual effective** station switches, per-stream interval, missing-AUTO policy, last AUTO/armed age and DM attempt age |
| Recurring DMs | Local-only configured poll slots | Four local recurring poll slots reported to Cloudflare |
| Remote DM changes | Cloudflare `mesh_gateway_dm_overrides`, checked by Heltec | Same existing implementation and supported intervals, no new DM behavior |
| Firmware OTA | Approved GitHub and KV images; signed manifest and protected recovery loader | Unchanged |
| Legacy Vercel/Neon and DYP Cloudflare routing | Working and separate | Unchanged |
| Local HTTPS diagnosis | TLS errors, heap and RSSI | Unchanged |

Heltec still waits **60 continuous minutes without the relevant automatic sensor packet** before a missing-AUTO rescue DM. Configured 15/30/45/60/360/720/1440 minute intervals are the **repeat cadence after missing AUTO**, not the sensor's own reporting interval. Pack Creek has two independently watched streams. Auto-DM is opt-in per watched node; Cloudflare's requested setting is not evidence it was applied until the gateway reports it.

### Watched auto-DM nodes

| Node | Node number | Stream |
| --- | --- | --- |
| Pack Creek | 1949224949 | Water and HOBO MX2001 |
| South Cottonwood (lost) | 1695962694 | Water |
| Mill Creek | 1683604927 | HOBO |
| Matheson Groundwater | 2004386937 | HOBO |
| Cottonwood Relay | 2425602407 | Temperature |
| Hidden Valley | 3044869407 | Temperature |
| Fishlake Hightop | 1577197109 | Temperature |
| Seed Relay 1 | 647393146 | Temperature |
| Thousand Lake Mountain | 2650172798 | Temperature |
| Mini Me | 4241345683 | Temperature |

**Do not enable lost South Cottonwood.** The `DYPA08 1` (`!55ca0677`) and `DYPA08 2` (`!636eb002`) are Cloudflare data collection nodes, not V9 auto-DM rescue targets.

## Cloudflare page and data contract

[CCES Monitor → Auto DM](https://cces-monitor.raspy-lake-a7f4.workers.dev/#dm) is owner-only. It deliberately separates:

- **Heltec reported**: effective state after the radio posted `auto_dm_state`, timestamped in D1 `mesh_gateway_dm_state`.
- **Cloudflare requested**: stored desired override in `mesh_gateway_dm_overrides`, with D1 config version increment.
- **Pending / stale**: override has been written but not yet confirmed by a new effective report; V11 does not report this field and must never be shown as confirmed.

`POST /api/mesh/dm` accepts only the ten firmware-known rescue node numbers, boolean enabled and the supported repeat intervals. Each change records an audit action; it does **not** trigger firmware OTA, factory reset, calibration change or a RAK reflash.

D1 `mesh_gateway_reset_diag` stores reported reset reason and formatted, retained diagnostic data. V12 reports to the existing authenticated `POST /v10/health` and does not require a new gateway URL or inbound port.

ESP32 codes include `POWERON`, `SOFTWARE`, `PANIC`, `INT_WDT`, `TASK_WDT`, `WDT`, `BROWNOUT`, and `DEEPSLEEP`. A 15-minute cloud health interval does not reset `millis()`, but power interruption or an ESP32 software/hardware reset does. Diagnose before modifying the supervisor. The existing system also has a 15-minute failed-upload recovery threshold, a 90-second task watchdog and an intentional 12-hour preventive reboot. The **actual reset reason is necessary** before blaming any one of them.

## Data flow and recovery limits

V12 keeps the Heltec Cloudflare client’s 12-second connect/HTTP timeouts, bounded 30/60/120/240-second config retry cadence, independent Cloudflare and Vercel upload queues, HOBO BLE and existing V9 DM controls.

Authenticated health payload is constrained below the existing Worker 4096-character limit. The Worker validates exactly 11 supported sensor-watch entries, four or fewer recurring polls, interval bounds and node numbers before storing the effective state. A malformed optional snapshot is ignored without blocking the essential OTA health acknowledgment.

As before, an explicitly approved signed, SHA-256-checked application BIN is staged **beyond the first 0xA0000 bytes** of the `CCA_PULL_1` loader in OTA_1, then installed into OTA_0. On a verified boot, Cloudflare disables remote OTA. **There is no complete automatic rollback for a syntactically valid app that later boot-loops.** Remote flashing is not a substitute for testing and a recovery route.

[Existing V11 diagnostic/OTA runbook](https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v11/README.md) · [Cloudflare firmware manager](cloudflare/cces-monitor/README-FIRMWARE-MANAGEMENT.md) · [V12 Cloudflare Worker](cloudflare/cces-mesh-remote/worker.js).
