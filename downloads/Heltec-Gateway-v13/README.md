# Heltec Gateway V13 | Build, flash, and field notes

## V13 remote LoRa reboot addition (November 26, 2026)

Send exactly `REBOOT` (case-insensitive) in a **PKI-encrypted direct message** to Heltec. The sender must match an existing configured Meshtastic admin public key. If no admin keys are configured, an explicitly favorited node on Heltec is accepted instead. Shared-channel messages, broadcast messages, unknown senders, and partial commands cannot initiate a reboot. A successful request queues a reply and schedules a normal full reboot 20 seconds later; an existing pending reboot is not replaced. No Wi-Fi or Internet is required. No credentials, partition, or stored settings are changed. The pre-existing recovery tests remain enabled, and CI additionally tests exact command parsing, authentication, and handler integration. Radio acknowledgement and actual device reboot must still be verified in the field. The repaired V13 download remains in the same folder; use the newest run-stamped BIN identified in BUILD.txt.

## November 26 V13 in-place network recovery repair

The initial V13 build is retained as historical evidence, not the recommended OTA. No V14 is created. The repaired image still reports `HELTEC-GATEWAY-V13`, but the OTA manifest must use a unique build identifier (e.g. `heltec-gateway-v13-repair-r<run>`), because the installed loader deduplicates the applied release ID, not the hash alone.

- Centralized application Wi-Fi reconnect requests; do not clear cached credentials on Heltec disconnects.
- Retry after 2 minutes of Wi-Fi loss, 10 minutes without validated Cloudflare configuration, or an upload-stall signal; throttle supervisor repairs to 5 minutes.
- First full reboot after 20 minutes of persistent Wi-Fi loss or management/config silence; persistent upload stall starts its own 20-minute backstop when detected. Retained repeat-outage intervals back off to 40 and 60 minutes, with no permanent lockout. Clear the outage counter after 15 minutes of sustained management health.
- Preserve the 90-second task watchdog, 12-hour preventive reboot, signed OTA/recovery loader, sensor allowlist, DM configuration and normal network-only recovery. A full reboot may still restart in-memory missing-AUTO timing and discard volatile queues. No physical recovery guarantee is possible for failed power or hardware.
- Ten host-side scenarios plus previous compatibility tests run in CI. Hardware fault-injection and field stability are not implied by CI success.

**Latest repaired download:** [V13 release folder](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13) and [BUILD.txt](https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/BUILD.txt). Use only the run-stamped BIN matching BUILD.txt after green CI. Original date-only BIN/ZIP links below refer to the initial V13, not the repair. No auto-approval from GitHub.


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
- Change custom `UploadStalled` / `WifiStalled` reactions to throttled network reconnects, before a bounded full MCU reboot backstop. The LoRa radio, automatic-DM scheduling, existing upload queues, and saved station settings remain up.
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
