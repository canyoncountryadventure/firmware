# Heltec Gateway V10 | Cloudflare + Remote Management

**Release target:** November 25, 2026 | Heltec WiFi LoRa 32 V4 OLED | ESP32-S3 | PlatformIO `heltec-v4`

**Base:** [V9 Clock](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary), the **last verified firmware before Cloudflare and remote management**. V9 Clock's firmware code, deployment binary, mesh routing, scheduled DMs and recovery remain the rollback reference. V10 does not replace the existing station systems merely because a Cloudflare pilot exists.

> **Release history:** The original V10 image from source `8bf417d` passed CI and is running on the field Heltec. Subsequent commits on this **same V10 branch** add experimental self-hosted OTA staging and an independent loader. These newer commits are NOT an approved field release until both CI workflows pass and the loader is bench-tested. Physical autonomous update and recovery validation remains pending.

## 1. V10 firmware downloads

- [V10 GitHub Actions: build and verification](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_cca_heltec_gateway_v10.yml)
- **[Download V10 non-factory OTA application BIN (November 25, 2026)](downloads/Heltec-Gateway-v10/Heltec-Gateway-v10-11-25-26.bin)**
- [Download V10 release ZIP](downloads/Heltec-Gateway-v10/Heltec-Gateway-v10-11-25-26.zip)
- [V10 source SHA and SHA-256 checksum manifest](downloads/Heltec-Gateway-v10/BUILD.txt)
- [V9 Clock verified rollback BIN and checksum](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary/downloads/Heltec-Gateway-v9-Clock)

After CI completes, download **`Heltec-Gateway-v10-11-25-26.bin`**, verify its SHA-256 against `BUILD.txt`, and flash through the existing Meshtastic Wi-Fi OTA method. The BIN is an **application-only file, not factory firmware**. Do not erase NVS, Wi-Fi credentials, channels, radio identity, HOBO settings, or keys.

```powershell
$ErrorActionPreference = "Stop"
$ip = "192.168.1.147" # replace if the Heltec address changed
$dir = "C:\HeltecUpdate"
New-Item -Path $dir -ItemType Directory -Force | Out-Null
$file = Join-Path $dir "Heltec-Gateway-v10-11-25-26.bin"
Invoke-WebRequest "https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v10/downloads/Heltec-Gateway-v10/Heltec-Gateway-v10-11-25-26.bin" -OutFile $file
if ((Get-FileHash $file -Algorithm SHA256).Hash -ne "6C4B2C95B4DE93C1FCBFCEA7D8025A96F886F089784303404ED3DA4C739D9B80") { throw "Wrong firmware hash - DO NOT FLASH" }
py -m meshtastic --host $ip --ota-update $file
Invoke-RestMethod "http://$ip/json/gateway" | ConvertTo-Json -Depth 8
```

Expected `firmware_fix`: `v10-cloudflare-remote-2026-11-25`. The JSON includes a new `cloudflare` object with config version, sync success/failure, registered node count, remote-DM revision, latest cloud HTTP status and OTA manifest status.

## 2. What changes from V9 Clock

| Feature | V9 Clock | V10 |
| --- | --- | --- |
| Existing Pack/Mill/Matheson/White Pine/relays | Vercel/Neon | **Still Vercel/Neon, unchanged** |
| First DYP station | Not accepted | `DYPA08 1` `!55ca0677` / `1439303287` → Cloudflare |
| Second DYP station | Not accepted | `DYPA08 2` `!636eb002` / `1668198402` → Cloudflare |
| Dynamic Cloudflare station list | None | Pull every 5 minutes; validated and cached in ESP32 NVS |
| Remote station assignments | None | Change in Cloudflare D1, then gateway sync, without reflashing |
| Remote V9 auto-DM settings | Local/DM | Optional Cloudflare overrides of the existing V9 watched stations |
| Gateway health | Local JSON | Local JSON plus Cloudflare health every 15 minutes |
| Firmware updates | Local/Meshtastic Wi-Fi OTA | Same working OTA **plus signed remote-manifest checks** |
| Unattended remote OTA install | Not present | **Experimental source only: gated by a separately installed CCA loader and Cloudflare approval** |

V10 **does not reprogram or reflash your RAK4631 V8 nodes**. They continue to trigger their own DYP-A08 sensors, use `WATER INTERVAL 15M` and `WATER SAMPLES 5`, and transmit 24-byte `DS` water packets. The Heltec is a receiving/forwarding gateway.

## 3. Cloudflare server

**Dedicated Worker:** `cces-mesh-remote`  \
**URL:** `https://cces-mesh-remote.raspy-lake-a7f4.workers.dev`  \
**D1 database:** `cces-monitor`, in distinct `mesh_*` tables.

| Endpoint | Purpose |
| --- | --- |
| `GET /v10/config` | Authenticated 5-minute pull for node IDs, destinations, auto-DM overrides and OTA manifest |
| `POST /v10/ingest` | Authenticated water distance and device/battery telemetry |
| `POST /v10/health` | Authenticated gateway health every 15 minutes |
| `GET /healthz` | Public service health only |

**Credential requirement:** The Cloudflare Worker must have a secret binding named `GATEWAY_KEY` with the same value as the Heltec build's existing `HOBO_HTTP_GATEWAY_INGEST_KEY`. The latter comes from the GitHub Actions repository secret. **Do not commit either key to GitHub.** Cloudflare rejects requests with HTTP 401 until the matching secret is provisioned. The original Vercel URL and credential still operate independently.

The Cloudflare D1 tables are `mesh_stations`, `mesh_observations`, `mesh_device_telemetry`, `mesh_gateway_config`, `mesh_gateway_dm_overrides`, and `mesh_gateway_health`.

**Routing rule:** If a node is on the authorized, cached Cloudflare pilot list, forward its water and device telemetry to Cloudflare; otherwise all existing V9 nodes use the original Vercel/Neon destination. In particular, **the two DYP nodes never create duplicate Neon/Vercel records**.

## 4. Remote configuration without reflashing

The gateway initiates an **outbound** Cloudflare config request every five minutes. It parses registered node IDs and their Cloudflare destinations, rejects malformed/rollback configurations, and saves valid IDs and their version in ESP32 NVS. Wi-Fi failure leaves the last good IDs in place. DYPA08 1 and DYPA08 2 are built into V10 as initial defaults.

Cloudflare's `mesh_gateway_config.version` must advance when the operator changes remote DM overrides. An optional `mesh_gateway_dm_overrides` entry can set an *existing V9 watched station* ON/OFF or set its missing-automatic timeout to **15, 30, 45, 60, 360, 720, or 1440 minutes**. Without an override the V9 Clock behavior remains. V10 does not silently turn on fallback DMs for the new DYP nodes.

To add an additional Cloudflare DYP node later: add a validated/enabled `mesh_stations` entry and increment the gateway configuration version. Once the next five-minute pull succeeds, it can upload automatically without a Heltec flash.

## 5. Remote firmware capability, boundaries, and safety

V10 checks a versioned Cloudflare OTA manifest with an approved HTTPS URL belonging to the firmware repository, a 64-character firmware SHA-256 and an **HMAC-SHA256 signature**. The D1 option `ota_enabled` defaults to **false**.

**Important distinction:** The **currently deployed** Heltec V10 validates remote manifests only; it cannot install new firmware itself. The updated source on this same branch implements a guarded downloader/stager that requires a new companion OTA_1 recovery loader with app descriptor `CCA_PULL_1`. It rejects unattended installation until the matching loader is installed and Cloudflare approves a signed release. The default D1 switch remains OFF. Do not install or enable the experimental updater until its source and loader builds pass and it passes physical tests of retry/power-loss/recovery behavior. No full autonomous rollback of a bootable-but-crashing application is implemented.

The remote service does not open inbound ports or expose Cloudflare admin actions to the public. Operational Cloudflare configuration changes are performed through authenticated account administration, not an unauthenticated web form.

## 6. Checks after V10 flash

1. Open `http://192.168.1.147/json/gateway` (use the actual Heltec IP); verify V10 marker, Wi-Fi health, and `cloudflare` status.
2. Confirm existing V9 stations **still upload to Vercel/Neon**. Confirm their automatic-DM clock settings remain as configured.
3. On `DYPA08 1` and `DYPA08 2`, separately DM `WATER TELEMETRY NOW` and check Cloudflare `mesh_observations`. A raw `distance_mm` is valid before calibration; **never manufacture stage from raw distance**.
4. Check device/battery packets in `mesh_device_telemetry` and periodic gateway health in `mesh_gateway_health`.
5. Leave unattended OTA disabled. Roll back to [V9 Clock](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary) if the existing legacy pipeline regresses.

| Symptom | First check |
| --- | --- |
| Cloudflare 401 | Missing/mismatched Worker `GATEWAY_KEY` secret binding |
| Remote config version 0 | First successful remote sync has not occurred; built-in DYP node IDs still exist |
| DYP packets reach radio but not D1 | Check V10 source/firmware identity, receiver packet upload, and Cloudflare authentication |
| Distances present but water stage absent | Calibrate a real reference stage on each RAK at its actual installation |
| Existing Neon data missing | Inspect the untouched legacy Vercel path separately; do not disable it |
| OTA does not install from Cloudflare | Expected: manifest checks available; automatic installer deliberately disabled |

## 7. Source and rollback

- [V10 source](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v10)
- [V10 Cloudflare Worker](cloudflare/cces-mesh-remote/worker.js)
- [V10 gateway/Cloudflare regression tests](test/test_heltec_v10/test_cloud_remote.py)
- [V10 build and dated releases](.github/workflows/build_cca_heltec_gateway_v10.yml)
- [V10 self-update companion OTA loader workflow](.github/workflows/build_cca_remote_loader.yml)
- [V10 CCA remote-loader design, requirements and recovery caveats](tools/cca_remote_ota/README.md)
- [V9 Clock last pre-Cloudflare release](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary)

**V9 Clock is the established rollback reference.** Changing V10 gateway firmware must not require reflashing, factory-resetting, or repartitioning working field RAK radios.

## 8. Experimental Heltec-only autonomous OTA (source under review)

The **same** `Heltec-Gateway-v10` source now contains two separately compiled components:

1. Main V10 app: `src/modules/CcaRemoteOtaStage.cpp`, validates approved signed Cloudflare policy, downloads the BIN and verifies its SHA-256, then stages the image in spare flash in OTA_1 after the loader's first 640 KiB.
2. Independent CCA OTA_1 companion loader: built from the pinned official Meshtastic Wi-Fi/BLE loader with the `tools/cca_remote_ota/` patch. The loader reads the verified staged image and writes OTA_0, with at most 10 recovery attempts. It preserves TCP port 3232 and BLE recovery.

**Do not flash the companion loader while still unvalidated.** The loader requires a one-time targeted USB flash at offset `0x650000` on Heltec V4, without erasing flash, changing partitions or deleting NVS. Flashing an unverified loader at the wrong offset could make the radio unusable. The current app-only Wi-Fi OTA update does not install the separate loader.

The [CCES monitoring website](https://monitor.canyoncountryecological.com/#mesh) includes admin-only **Remote Management** controls for registering Cloudflare DYP nodes, setting watched-station automatic-DM overrides, and saving approved firmware metadata. Cloudflare rejects **enabling** unattended OTA until a fresh Heltec health report confirms the matching CCA recovery loader is installed. New firmware is never automatically enabled merely by publishing to GitHub.

**Recovery limitations:** The loader can recover a partial write to OTA_0 while it stays bootable in OTA_1, and can accept a local port 3232 upload if automatic retry fails. But a new app that passes image validation then crashes may require onsite repair because there is no retained old app image for a true automatic rollback. Until physically bench-tested, the entire update system must stay disabled in production.

Use the [app build action](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_cca_heltec_gateway_v10.yml) and [companion loader build action](https://github.com/canyoncountryadventure/firmware/actions/workflows/build_cca_remote_loader.yml) to verify current source. A dated downloadable firmware is trustworthy only when its BUILD.txt SOURCE_COMMIT matches the current intended source and CI is green.
