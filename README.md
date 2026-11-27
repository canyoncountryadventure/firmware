# CCES Heltec Gateway V13 | Stable auto-DM and recovery repair

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


**Heltec WiFi LoRa 32 V4 (ESP32-S3) | PlatformIO `heltec-v4` | November 26, 2026**

**Status:** CI build candidate. Publishing this GitHub branch **does not** authorize or install firmware. The initial V13 was field-installed; this revised build requires separate approval of its exact SHA-256-pinned release in Cloudflare.

## Downloads, checksums, and build provenance

- [V13 application-only .BIN](https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-26-26.bin)
- [V13 ZIP with companion firmware files](https://github.com/canyoncountryadventure/firmware/raw/refs/heads/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/Heltec-Gateway-v13-11-26-26.zip)
- [V13 BUILD.txt with SHA-256, exact source commit, and Actions run](https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/BUILD.txt)
- [V13 firmware release notes](https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v13/downloads/Heltec-Gateway-v13/README.md)
- [V13 GitHub Actions build and publish workflow](https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v13/.github/workflows/build_cca_heltec_gateway_v13.yml)
- [Last field-confirmed V12 release](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v12/downloads/Heltec-Gateway-v12)
- [V9 Clock, the last build before Cloudflare remote integration](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary)

**Do not use download links until CI has published the actual files and BUILD.txt.** Validate the BIN hash against BUILD.txt and Cloudflare's release record. The built BIN is application-only; never factory erase NVS or overwrite the independent `CCA_PULL_1` recovery loader in OTA_1.

## Why V13

Cloudflare V12 boot/reset diagnostics showed repeated `TASK_WDT(6)` resets, including a loop stall near 930 seconds, around the 15-minute health reporting boundary. Vercel/Neon packet supervisor telemetry also showed `last_recovery_reason=5` (custom `UploadStalled`), and ESP32 reported a separate `SOFTWARE(3)` reset after ~3614 seconds. These are distinct root causes, and the specific network call behind TASK_WDT was not established in V12.

A full gateway reboot interrupts LoRa reception, flushes in-memory scheduling, and delays missing-AUTO rescue DMs at least another 60 minutes. A network failure should not reset a still-functional mesh radio.

## Changes compared with V12

1. **Keep the ESP32's 90-second task watchdog.** It still resets a truly hung Arduino loop task; do not hide stalls by lengthening it.
2. **Repair HTTPS/Wi-Fi without reboot.** The custom V7 supervisor's `UploadStalled` and `WifiStalled` signals now cause throttled Wi-Fi-only reconnects (minimum five minutes apart), without immediately rebooting the MCU, LoRa stack, or sensor scheduler; the repair adds a timed full-reboot backstop. `ModuleStalled` and genuine mesh RF failures keep their existing guarded recovery.
3. **Prevent false recovery-counter rearm.** Reboot protection is cleared only after a full hour of uptime with *recent* radio RX and an accepted Vercel/Neon POST in the last five minutes. Old successes no longer count.
4. **Bound TLS handshakes separately from HTTP reads.** Cloudflare config GET (12 sec), health POST (8 sec), and legacy/Cloudflare sensor POST (12 sec) have explicit WiFiClientSecure handshake limits, connect limits, and existing read limits. These are intentionally much shorter than the 90-second watchdog.
5. **Retain the currently active HTTPS operation across watchdog resets.** Reset diagnostics can now say `CLOUD_CONFIG_TLS`, `CLOUD_HEALTH_TLS`, or `SENSOR_UPLOAD_TLS` when the task dies while one of those synchronous calls is active.
6. **Expose recovery evidence.** Cloudflare `mesh_gateway_health.note` adds `v13_net_repair`, last network recovery cause, HTTP status, queue depth, and last successful upload age; Vercel/Neon supervisor telemetry adds the equivalent counters.
7. **Keep OTA and sensor behavior intact.** No change to the OTA loader, partitions, NVS, sensor allowlist, BLE, DM commands/intervals, RF modem settings, LoRa hop behavior, Vercel and Cloudflare upload routing, or the 60-minute missing-AUTO gate.

## Remote auto-DM and expected behavior

Auto-DM rescue is opt-in for Pack Creek water and HOBO, Mill Creek HOBO, Matheson HOBO, Cottonwood Relay temperature, and Fishlake Hightop temperature as configured on the radio. Other watched stations are controlled through their existing saved presets or Cloudflare overrides. **A rescue only qualifies after a continuous full hour without a true automatic sensor packet**, then fires at the selected wall-clock boundary; an interval change can restart that hour for the affected station. Automatic battery/device packets are **not** substitutes for missing sensor telemetry. Replies received through a manual or automatic DM are tagged `heltec_dm` in the Vercel/Neon data path.

The new network recovery intentionally preserves all radio-side timers. A genuine watchdog reset still resets those timers; V13 logs the in-flight TLS operation to help locate its cause.

## Tests and acceptance

V13 GitHub Actions must pass existing V5–V12 compatibility/OTA tests, the host C++ supervisor scenario tests, and V13 source guardrails, before publishing the date-stamped BIN and BUILD.txt. A successful build **does not** prove a field fix.

After an explicitly approved remote installation, confirm in Cloudflare:
- `firmware=HELTEC-GATEWAY-V13`, with the applied release identifier and OTA authorization automatically disabled.
- Continuous uptime **over 90 minutes** through multiple 15-minute Cloudflare heartbeats, with no new `TASK_WDT`.
- An enabled station missing automatic sensor data for at least an hour receives rescue DMs on its chosen interval.
- Vercel/Neon accepts `heltec_dm` packets, and Cloudflare/Vercel network recovery counters remain bounded.

If the gateway still resets, inspect `mesh_gateway_reset_diag` for the new retained TLS operation name; do not blindly increase watchdog timeout. A reliable V12 binary/BUILD.txt remain available for deliberate rollback, through the same protected OTA process.

## Safe deployment

**No auto-deploy:** Publishing code or CI artifacts does not update Cloudflare `mesh_gateway_config` or approve OTA. Only explicitly authorized operator workflow may stage the signed SHA-256 image. Use the existing Cloudflare [Firmware Management](https://monitor.canyoncountryecological.com/#firmware) panel after reviewing BUILD.txt. Never erase the recovery partition or factory-reset field settings.
