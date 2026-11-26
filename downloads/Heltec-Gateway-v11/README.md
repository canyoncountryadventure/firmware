# Heltec Gateway V11 — Cloudflare firmware library
Release date: November 25, 2026 (America/Denver)
Hardware: Heltec WiFi LoRa 32 V4 (ESP32-S3)
Build environment: `heltec-v4`

## Purpose
V11 is the successor to the proven V10 gateway build 24. V10 (SHA-256
`33551a8376cc3e391743c27b417932f28efc064a4a3d11ba24f2b39fd7ee87f3`)
successfully completed a cloud-initiated OTA upgrade with no USB cable,
passing `verified-boot` using the preinstalled CCA recovery loader.

V11 **adds support for Cloudflare KV-hosted firmware binaries**, in addition
to GitHub releases. Firmware uploads are managed in the existing CCES Monitor
dashboard under **Firmware**. Each accepted BIN is SHA-256 verified and stored
under an unguessable 64-hex-token URL. The Cloudflare Worker signs release
manifests using the existing gateway key. The Heltec checks that signature,
validates the approved Cloudflare host and token form, checks the image SHA,
stages it beyond the protected loader region, reboots into the independent
loader, and verifies the new app before choosing it as the boot target.

Cloudflare configuration, node allowlists, V9 auto-DM policy, dual Vercel/
Cloudflare telemetry routing, HOBO BLE handling, and Wi-Fi OTA remain.

## First-time transition from V10
1. Build and publish the dated V11 BIN via GitHub Actions.
2. Register its GitHub **raw BIN** URL and exact SHA-256 in Firmware Management.
3. Explicitly approve the GitHub V11 release. V10 already knows how to
   download GitHub BIN files and perform cloud-initiated OTA.
4. Confirm `verified-boot` on Heltec, plus a Cloudflare health note containing
   `fw_kv=1`. After V11 boots, Firmware Management permits approvals of
   Cloudflare KV uploads too.
5. Turn remote OTA approval OFF again (the gateway continues to sync ordinary
   configuration every five minutes).

**No new USB loader flash or partition erase is required.** Keep the separate
CCA loader at OTA_1 offset 0x650000; preserve the NVS identity and mesh keys.

## Safety limitations
- A corrupted or mismatched SHA-256 is rejected before installation.
- The loader occupies the beginning of OTA_1, with staging after 0xA0000.
- The loader retries interrupted installation up to 10 times, otherwise
  remains available for local recovery.
- There is **no fully automatic rollback for an image that boots but crashes
  afterward**. Do not mistake local recovery for fail-safe unattended rollback.
- Cloudflare OTA is **off unless explicitly approved**. Never publish a test
  image as approved by default.
- KV is a small firmware library for this deployment. R2 is not enabled.
