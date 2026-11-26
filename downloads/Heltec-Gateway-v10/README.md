# Heltec Gateway V10 • November 25, 2026

**Historical release reference, superseded by [V11](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v11).**

V10 was the **first Heltec gateway release with Cloudflare D1 integration and remote configuration**, built on the verified V9 Clock station routing and missing-automatic DM scheduling. V9 Clock remains the last firmware **before Cloudflare and remote management**. V11 has now been installed in the field with the independent CCA remote loader; the V10 rollout is no longer the active production target.

## Exact currently published V10 files

| File | Manifest SHA-256 |
| --- | --- |
| [`Heltec-Gateway-v10-11-25-26.bin`](Heltec-Gateway-v10-11-25-26.bin) | `45b4218ee591d142d1193918e16cc6e18e5343520761ed1c352e1573d46979fe` |
| [`Heltec-Gateway-v10-11-25-26.zip`](Heltec-Gateway-v10-11-25-26.zip) | `8c25bb0e68216e0bf9ac9dbeb5a93db4f2bebafea07fecbd4af7ac5d0a42a5fd` |
| [`BUILD.txt`](BUILD.txt) | Source commit `b666ce18207acb2a3d391e5c170a3ea47b7a099f`; [GitHub Actions run 45762097828](https://github.com/canyoncountryadventure/firmware/actions/runs/45762097828) |

Build target: **Heltec WiFi LoRa 32 V4 OLED**, ESP32-S3, PlatformIO `heltec-v4`; release BIN size **2,231,856 bytes**. Use the **application-only BIN**, not a factory image, if deliberately rolling back or recovering.

### Critical distinction: field-verified V10 build 24 vs current V10 download

An earlier remote upgrade installed `heltec-v10-downloaderfix-cloud-test-b24` and recorded a **different binary SHA-256**:

- **Verified installed build 24:** `33551a8376cc3e391743c27b417932f28efc064a4a3d11ba24f2b39fd7ee87f3`.
- **Current dated V10 branch BIN:** `45b4218ee591d142d1193918e16cc6e18e5343520761ed1c352e1573d46979fe`.

They are **not the same binary**, despite the similar firmware URL/filename. Never claim that downloading the present V10 branch BIN restores the exact field-installed build 24. Verify file hash and provenance; an archival build 24 binary must be independently located and validated if it is needed.

## Features V10 introduced

1. Cloudflare-only forwarding for `DYPA08 1` (`!55ca0677` / `1439303287`) and `DYPA08 2` (`!636eb002` / `1668198402`).
2. Outbound `GET /v10/config` every five minutes. Cloudflare D1 manages those node IDs, optional overrides for pre-existing V9 auto-DM watched stations and release approval.
3. `POST /v10/health` at a nominal 15-minute interval; `POST /v10/ingest` for approved DYP distance/battery packets.
4. Cached Cloudflare node settings in NVS, with legacy V9 Meshtastic station allowlist, clock-triggered automatic DMs, manual READs, HOBO BLE and Vercel/Neon forwarding retained.
5. Signed SHA-256-pinned remote OTA metadata, and an application-side stager requiring the independent `CCA_PULL_1` recovery loader in the Heltec's OTA_1 partition.
6. The stager protects the first `0xA0000` bytes of OTA_1, verifies the image, then invokes the independent loader to install into OTA_0. No routine OTA install is allowed without explicit authorization.

The V10 build-24 downloader fix successfully completed an approved remote V10-to-V10 test before V11. **The earlier V10 documentation's claim that automatic installation had never been tested became outdated after that test.**

## Preserved station routing

| Source | Destination |
| --- | --- |
| Existing V9 Creek, HOBO and relay nodes | SE Utah Watersheds / Vercel/Neon |
| DYPA08 1 and DYPA08 2 | Cloudflare D1 only |

The gateway does **not** recalibrate, rename, or flash the RAK4631 nodes. Raw ultrasonic distance is not stage before calibration. Existing automatic-DM interval settings and manual commands remain distinct from the remote Cloudflare node list.

## Why V11 replaced V10

The V10 client showed intermittent Cloudflare configuration GET failures, including `HTTP=-1`, while continued Vercel telemetry confirmed that the legacy path could work independently. V11 improved config HTTPS connection timeouts, added retries after 30/60/120/240 seconds, richer local TLS error and heap/RSSI diagnostics, and support for KV firmware downloads. V11 **does not by itself establish that the root cause of all TLS failures has been eliminated**.

On November 26, 2026, V11 was verified to boot and post authenticated health, and the remote OTA authorization turned off automatically afterward.

## Recovery and safety

- Keep the CCA loader `CCA_PULL_1` in OTA_1 at base offset `0x650000`, protecting its first `0xA0000` bytes from firmware staging.
- Do **not** factory erase, repartition, clear NVS, or lose the Meshtastic radio key/channel identity to perform an ordinary application update.
- The loader can retry certain interrupted installations but there is **no full automatic rollback** if a new, valid application later crashes.
- V10 never supported direct Cloudflare KV-hosted BIN download URLs; it only accepted approved firmware GitHub URLs. **V11** added KV host support and advertises `fw_kv=1`.
- Firmware-version changes to Heltec do not update the remote RAK sensors.

## Reference pages

- [V10 branch source and full architecture](../../README.md)
- [V11 successor: verified dated release and troubleshooting](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v11/downloads/Heltec-Gateway-v11)
- [V9 Clock last pre-Cloudflare](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary)
- [V10 manifest and exact hash](BUILD.txt)
