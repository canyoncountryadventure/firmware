# Heltec Gateway V13 | Build, flash, and field notes

[Home and full engineering README](https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v13/README.md) | [Build workflow](https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v13/.github/workflows/build_cca_heltec_gateway_v13.yml)

## Application downloads (published by green CI only)

- [Application-only BIN: Heltec-Gateway-v13-11-26-26.bin](https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-26-26.bin)
- [ZIP archive with companion flash artifacts](https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-26-26.zip)
- [BUILD.txt: SHA-256 checksums, source commit, and Actions run](https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/BUILD.txt)
- [Previous V12 field-confirmed firmware for intentional rollback](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v12/downloads/Heltec-Gateway-v12)
- [Last pre-Cloudflare V9 Clock firmware](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary)

Until the CI publish step succeeds these V13 BIN/ZIP/BUILD links may not exist. Never approve an OTA manifest based only on this README. Verify each SHA and source SHA first.

## Hardware and update contract

**Target:** Heltec WiFi LoRa 32 V4, ESP32-S3, PlatformIO `heltec-v4`. Use only the application BIN for a signed, SHA-256-pinned OTA through the pre-existing `CCA_PULL_1` loader. **Never upload a factory image, erase NVS, overwrite the OTA_1 reserved loader region, or change credentials.** No OTA approval occurs automatically.

## Changes from V12

- Preserve the ESP32 90-second task watchdog for actual hung tasks.
- Change custom `UploadStalled` / `WifiStalled` reactions to throttled network reconnects, not entire MCU reboots. The LoRa radio, automatic-DM scheduling, existing upload queues, and saved station settings remain up.
- Correct the prior recovery-reboot guard: a stale accepted POST alone no longer counts as proven current health.
- Bound Wi-Fi TLS handshake separately from HTTP connect/read on Cloudflare config, health, and sensor ingestion.
- Retain in-flight TLS operation names in reset diagnostics to pinpoint future watchdog events.
- Put last network recovery reason/status/queue and last successful upload age into Cloudflare heartbeat notes and Vercel supervisor packet metadata.
- Preserve V12 auto-DM actual-state snapshots, signed OTA, dual-ingest architecture, on-radio control DMs, and one-hour missing-AUTO gate.

## Validation and field rollout

1. Check the workflow is green and BUILD.txt hash matches the exact published BIN; keep V12 rollback available.
2. **Only on separate authorization**, register V13 in Cloudflare and approve the signed OTA image. Publishing does not install.
3. Confirm `HELTEC-GATEWAY-V13` on the next Cloudflare heartbeat; never infer successful installation from a 201 ingest response alone.
4. Observe at least 90 minutes with no task-watchdog resets; confirm hourly automatic rescue DMs actually produce `heltec_dm` readings in Vercel/Neon.
5. If TASK_WDT recurs, examine `CLOUD_CONFIG_TLS`, `CLOUD_HEALTH_TLS`, or `SENSOR_UPLOAD_TLS` in `mesh_gateway_reset_diag`, and use HTTP/queue counters to isolate the failing operation. Keep the true watchdog enabled.
