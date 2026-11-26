# CCA Heltec V4 Independent Recovery Loader

**Operational design and field evidence • Updated November 26, 2026**

The independent CCA loader, version descriptor **`CCA_PULL_1`**, runs in the ESP32-S3 **OTA_1 application partition** of a Heltec WiFi LoRa 32 V4. The main Meshtastic gateway app runs in **OTA_0**. Keeping a separate bootable loader makes it possible to replace OTA_0 using an approved firmware image without rewriting the loader itself.

## Field evidence: this is no longer an untested concept

- In November 2026, the field Heltec successfully installed the CCA loader once and remotely completed the V10 build-24 download/stage/install/boot sequence. The application reached local `verified-boot`.
- **November 26, 2026 at 10:36:23 AM MST:** the same loader was used for a **V10-to-V11 remote upgrade**, no onsite USB access. Cloudflare received an authenticated V11 heartbeat with `loader_ready=1,phase=3,attempts=0,applied=heltec-v11-cloudflare-tls-20261126,fw_kv=1`. The Worker wrote a `verified-boot` audit event and disabled OTA in config V8.
- These events prove successful **specific installations**. They do **not** prove all possible interrupted-write, boot-loop or full power-loss failure modes, nor long-duration stability of V11. A later `phase=4` heartbeat must be separately observed to assert the client recorded cloud confirmation.

## Layout (Heltec V4 16 MiB flash)

| Area | Use |
| --- | --- |
| OTA_0 base `0x10000` | Running Meshtastic/CCA application |
| OTA_1 base `0x650000` | Independent CCA loader, app descriptor begins with `CCA_PULL_1` |
| OTA_1 bytes `0x0` to `0x9FFFF` | **Reserved for the independent loader (640 KiB)** |
| OTA_1 offset `0xA0000` and later | Staging area for signed/verified application BIN |
| NVS namespace `cca_remote_ota` | Staging state, target release, attempts, applied version and errors |
| NVS namespace `MeshtasticOTA` | Loader handoff data; preserve identity and networking settings |

**Never flash a regular gateway application over OTA_1 at `0x650000`, use a factory erase, or repartition the existing field gateway.** Routine remote upgrades only stage bytes after the loader's protected first `0xA0000` bytes, then allow the loader to rewrite OTA_0.

## Controlled OTA sequence

1. Operator verifies the Heltec V4 application-only GitHub Actions release, SHA-256, source commit and successful CI; Cloudflare D1 `mesh_gateway_config.ota_enabled` is **normally false**.
2. Operator explicitly approves one release, after confirming a recent authenticated `loader_ready=1` heartbeat. V10's downloader accepts the approved GitHub host; V11 additionally supports an opaque Cloudflare KV URL.
3. Gateway polls outbound `GET /v10/config`, verifies HMAC-SHA256 signature over the firmware version, URL and SHA-256. The `/v10` route name remains in V11 for compatibility.
4. Gateway checks target vs already applied, validates partition scheme and loader descriptor; downloads the application into OTA_1 **after** reserved space and verifies SHA-256 from flash.
5. Gateway saves staging state in NVS, selects the loader as boot target and reboots. The loader re-verifies staged data and writes OTA_0. Installation retries are bounded by `MAX_STAGING_ATTEMPTS=10`.
6. The new app boots OTA_0 and sends an authenticated Cloudflare health POST. Matching `phase=3` confirms the installed app booted. On the first accepted health POST, local `markCloudConfirmed()` sets `phase=4` and the Cloudflare Worker disables the one-shot OTA authorization.
7. Verify OTA is off, a later local or Cloudflare phase-4 report, up-to-date config, and continued data receipt in **SE Utah Watersheds** and the Cloudflare DYP pilot.

This sequence preserves the field radio's Mesh identity, LoRa credentials, Bluetooth/HOBO settings and other NVS data. It does not remotely flash RAK4631 sensors.

## Loader provisioning on additional hardware

This is a **one-time separate installer** on **each new Heltec** that lacks `CCA_PULL_1`. The running app refuses to stage autonomously if the loader is not present and verified. An approved, audited loader may be provisioned by a controlled local USB process at OTA_1's correct `0x650000` base, without factory erase or NVS loss. Verify target hardware, binary size and descriptor **before** doing this. Do not infer that ordinary app Wi-Fi OTA installs the separate loader.

The loader build is sourced from the pinned official Meshtastic unified OTA implementation plus the CCA staged-image reader overlay. [Loader workflow](../../.github/workflows/build_cca_remote_loader.yml) and [application OTA source](../../src/modules/CcaRemoteOtaStage.cpp). Local port 3232 and BLE loader recovery can remain available, but a failure that leaves Wi-Fi unavailable may still need onsite access.

## Limits and threats

- **No fail-safe old-app rollback:** the loader overwrites OTA_0, so a new app can pass SHA validation and boot once, then crash repeatedly. The previous OTA_0 firmware is not kept as a rollback image.
- **Power loss / interrupted installation:** the independent loader can retry from preserved staged data, subject to its status and retry cap, but every failure path has not been tested on the field gateway.
- **Connection errors:** V11's configuration retry backoff and TLS diagnostics do not change how the loader copies flash. Periodic Cloudflare heartbeats may be delayed if Wi-Fi/TLS is unreliable.
- **Transport identity:** the gateway's HTTPS client presently uses `setInsecure()`, so the server TLS certificate is not validated. OTA manifest HMAC and staged flash SHA-256 supply additional release integrity, but ordinary configuration is not fully signed.
- **Recovery loader preservation:** reusing the app's generic OTA updater without respecting the partition map could overwrite the loader; use only the validated CCA staged-OTA path.

## Verify status

On the gateway LAN or VPN:

~~~powershell
$c = (Invoke-RestMethod "http://192.168.1.147/json/gateway").cloudflare
$c.ota_stage | Format-List *
$c | Select-Object remote_config_version,last_http_status,last_config_error,sync_ok,sync_failed
~~~

Expected fields: `loader_ready=true`, `state=installed` for phase 3 then `state=verified-boot` for phase 4, `attempts=0` for the observed V11 upgrade, and `applied=heltec-v11-cloudflare-tls-20261126`. The state is separate from config version and from SE Utah Watersheds telemetry.

See [V11 release README](../../downloads/Heltec-Gateway-v11/README.md), [main gateway README](../../README.md) and [Cloudflare firmware manager](../../cloudflare/cces-monitor/README-FIRMWARE-MANAGEMENT.md).
