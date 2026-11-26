# Heltec Gateway V12 Release | 2026-11-26

**Hardware:** Heltec WiFi LoRa 32 V4 / ESP32-S3. **Build target:** PlatformIO `heltec-v4`.

This is a proposed next application release after the remotely installed V11. Its purpose is observability, not changing radio transmission, DM default policies or reboot thresholds. **A GitHub publish event is NOT an installation authorization.**

## Binary downloads and verification

- [Application BIN, if published by CI](Heltec-Gateway-v12-11-26-26.bin)
- [Release ZIP, if published by CI](Heltec-Gateway-v12-11-26-26.zip)
- [BUILD.txt, mandatory exact SHA-256/source metadata](BUILD.txt)
- [V12 release workflow](../../.github/workflows/build_cca_heltec_gateway_v12.yml)
- [Main V12 architecture and operations](../../README.md)
- [Last remotely boot-confirmed V11 BIN, rollback reference](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v11/downloads/Heltec-Gateway-v11)

**Safety gate:** Do not select or flash these URLs until all CI jobs pass and `BUILD.txt` identifies the actual source commit, dated binary and SHA-256. The date in filenames comes from the GitHub Action's America/Denver build timestamp. Do not use a factory/erase image or overwrite the loader/NVS.

## Changes

1. **ESP32 reset diagnosis over the mesh gateway health endpoint:** sends `reset_reason_code` and a retained `HELTEC V4 DIAG` formatted record, with last reboot reason, prior runtime and any saved abnormal event. Cloudflare stores it separately in `mesh_gateway_reset_diag`. This allows remote discrimination between `SOFTWARE`, `PANIC`, watchdog, `BROWNOUT`, `POWERON` and other reset types.
2. **Actual running auto-DM state:** gateway serializes its existing 11 watched sensor streams across ten nodes and four separately configured poll slots. The `auto_dm_state` snapshot reports local effective switch, repeat interval, minimum 60-minute missing-AUTO gate and last observed or armed ages, not just Cloudflare's requested values.
3. **Cloudflare page:** the owner-only [Auto DM page](https://cces-monitor.raspy-lake-a7f4.workers.dev/#dm) displays requested vs actual states and makes audited remote override changes. V11 can already accept remote overrides, but only V12 can report full effective state.
4. **No routing or safety behavior changes:** V9 auto-DM presets and manual commands, HOBO BLE, both existing Vercel/Neon and DYPA08 Cloudflare pipelines, Wi-Fi/TLS config retry cadence, signed OTA manifest, protected independent OTA loader, and existing recovery timers are unchanged.

## Restarts: what is and isn't known

At 10:53, 11:08 and 11:23 AM MST on November 26, V11 Cloudflare heartbeats reported only 29–33 seconds of uptime. The gateway was rebooting on an approximately 15-minute cycle. Disconnecting/reconnecting Wi-Fi **cannot reset the ESP32 `millis()` uptime clock**.

Candidates in existing firmware include a V7 supervisor's 15-minute failed-upload stall recovery, application task watchdog (90 seconds after a stall), the scheduled 12-hour reboot (not a 15-minute explanation), LoRa recovery and power/brownout. Recent Vercel packet supervisor counters did **not** show an intentional V7-recovery reboot, so the 15-minute supervisor is a suspect, not a confirmed explanation. Only actual preserved ESP32 reset code from V12 will narrow the cause remotely.

## Operational precautions

- The signed, SHA-256-pinned release and independent `CCA_PULL_1` OTA loader remain mandatory for a remote install.
- The loader is preserved in OTA_1, with the first `0xA0000` bytes reserved. Normal V12 app updates must not overwrite or factory erase OTA_1 or NVS.
- No fully automatic rollback exists if a valid new application crashes later. Do not repeatedly approve OTA without a verified loader and a live gateway.
- Remote DM changes affect only ten preexisting V9 watched node numbers; South Cottonwood is physically lost and must remain OFF.
- `DYPA08 1` (`1439303287`) and `DYPA08 2` (`1668198402`) remain Cloudflare data-only stations, not rescue-DM targets.
- The gateway retains `setInsecure()` for HTTPS. The OTA manifest has a separate HMAC-SHA256 signature; full TLS certificate verification remains an outstanding hardening task.

## Useful references

[Cloudflare remote-control Worker](../../cloudflare/cces-mesh-remote/worker.js) · [Gateway reset reporter](../../src/modules/CcaCloudRemote.cpp) · [Local DM serializer](../../src/modules/HoboHttpGatewayModule.cpp) · [ESP32 retained reset diagnostics](../../src/platform/esp32/main-esp32.cpp) · [Main V12 readme](../../README.md).
