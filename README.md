# CCES Heltec Gateway V11

**Production build, November 26, 2026 | Heltec WiFi LoRa 32 V4 (ESP32-S3) | PlatformIO `heltec-v4`**

This is the authoritative V11 documentation for the **Heltec Moab Meshtastic gateway**, serving the existing SE Utah Watersheds (Vercel/Neon) pipeline plus a separate Cloudflare D1 pilot for two DYPA08 radios.

> **Confirmed remote installation:** On November 26, 2026 at 10:36:23 AM MST, the Heltec reported `HELTEC-GATEWAY-V11` with `phase=3,attempts=0,applied=heltec-v11-cloudflare-tls-20261126,fw_kv=1`. The authenticated Cloudflare Worker accepted this first V11 heartbeat, logged a server-side `verified-boot` and automatically turned OTA off by advancing its configuration from V7 to V8. This proves the first authenticated V11 boot, **not** long-term stability or an independently observed phase-4 heartbeat.

## Download the actual tested release

- [**V11 application BIN**](downloads/Heltec-Gateway-v11/Heltec-Gateway-v11-11-26-26.bin) and [release ZIP](downloads/Heltec-Gateway-v11/Heltec-Gateway-v11-11-26-26.zip)
- [Release-specific README](downloads/Heltec-Gateway-v11/README.md)
- [BUILD.txt with source SHA and SHA-256](downloads/Heltec-Gateway-v11/BUILD.txt)
- [Successful GitHub Actions run 45866228411](https://github.com/canyoncountryadventure/firmware/actions/runs/45866228411)

| Item | Exact value |
| --- | --- |
| BIN | `Heltec-Gateway-v11-11-26-26.bin` |
| Size | **2,232,880 bytes** |
| SHA-256 | `f3d7564a78e44fc2487982c0fa9dcfe267cd948ee08689fad956d724d66b66cc` |
| Compiled source commit | `6bd896c3a557846325492296f55b643286d17d12` |
| Build target | `heltec-v4` (ESP32-S3) |
| Build | Compatibility tests, compilation and publishing all passed |

Subsequent **documentation-only commits do not modify the published BIN**. Compare the actual file SHA-256 and `BUILD.txt` before flashing. This BIN is an **application only**, not a factory/erase image.

~~~powershell
(Get-FileHash "C:\HeltecUpdate\Heltec-Gateway-v11-11-26-26.bin" -Algorithm SHA256).Hash.ToLowerInvariant()
# Expected f3d7564a78e44fc2487982c0fa9dcfe267cd948ee08689fad956d724d66b66cc
~~~

## What V11 actually changed

| Behavior | V10 | V11 |
| --- | --- | --- |
| Cloudflare configuration sync | One attempt every five minutes | Same normal cadence; failure retries **30, 60, 120, then 240 seconds** |
| HTTPS request timeouts | 10 seconds | Explicit 12-second socket, connect and HTTP timeouts |
| Cloudflare diagnostics | HTTP status and success/failure count | `last_config_error`, `last_health_http_status`, `tls_free_heap`, `tls_largest_heap_block`, `tls_wifi_rssi_dbm` |
| Remote firmware URLs | GitHub repository | GitHub plus Cloudflare KV opaque firmware URLs |
| Signed OTA manifest and SHA-256 verification | Yes | Preserved |
| Independent recovery loader / staging beyond its reserved flash | Yes | Preserved |
| V9 manual DM, auto-DM, HOBO BLE, existing Vercel/Neon routing | Yes | Preserved |

**Do not claim V11 definitively fixes HTTP -1.** It improves retry speed and diagnostics but has not identified whether the underlying issue was a TLS/socket connection, DNS, Wi-Fi, heap, or HTTPS contention. Failed/successful sync counters reset on boot. Continued field monitoring is still necessary.

## Station identities and telemetry isolation

- **Legacy stations:** Existing SE Utah Watersheds Vercel/Neon telemetry path remains in service, including Pack Creek, Mill Creek, Matheson Groundwater, Hidden Valley, Fishlake and relevant relays. The Heltec keeps the original allowlist, V9 clock/missing-automatic DMs, manual READ commands, HOBO BLE and saved settings.
- **DYPA08 1:** `!55ca0677`, node `1439303287`, Cloudflare only.
- **DYPA08 2:** `!636eb002`, node `1668198402`, Cloudflare only.
- Cloudflare pilots use the RAKs' 24-byte `DS` water-distance packet plus device telemetry. `distance_mm` **is not** a calibrated stage; leave stage unavailable until a real site reference calibration. V11 does **not** change or flash the RAKs.

## Cloudflare deployment

- Gateway API Worker: `cces-mesh-remote` at `https://cces-mesh-remote.raspy-lake-a7f4.workers.dev`.
- Administrator dashboard: [Firmware Management](https://cces-monitor.raspy-lake-a7f4.workers.dev/#firmware).
- D1: `cces-monitor`, with `mesh_stations`, `mesh_gateway_config`, `mesh_gateway_health`, `mesh_gateway_dm_overrides`, `mesh_observations` and firmware release/action logs.
- KV: `cces-heltec-firmware`, bound as `FW_BIN`.
- V11 deliberately retains the **`/v10` URL prefix for API compatibility**: authenticated `GET /v10/config`, `POST /v10/health`, `POST /v10/ingest` and opaque download `GET /v10/firmware/<64-hex-token>`. Public `GET /healthz` tests the Worker, **not** gateway reachability.
- Cloudflare auth secret `GATEWAY_KEY` must match the firmware build secret `HOBO_HTTP_GATEWAY_INGEST_KEY`; do not expose either.
- OTA is **disabled unless a specific release is explicitly approved**. A matching authenticated first-boot health event disables it again.

**Security limitation:** Existing `WiFiClientSecure::setInsecure()` remains, so the ESP32 does **not validate the remote server certificate**. The OTA manifest has its own HMAC-SHA256 signature and binary SHA-256 integrity check, but that does not sign every ordinary configuration field. Certificate validation and full config authentication still require hardening.

## How to diagnose remotely and locally

On the local Wi-Fi LAN (or VPN to it), read the Heltec's gateway JSON:

~~~powershell
$c = (Invoke-RestMethod "http://192.168.1.147/json/gateway").cloudflare
$c | Select-Object build,remote_config_version,sync_ok,sync_failed,last_http_status,last_config_error,last_health_http_status,tls_free_heap,tls_largest_heap_block,tls_wifi_rssi_dbm,loop_stack_min_free_bytes,ota_stage
~~~

Interpret the fields carefully:

- `sync_ok` / `sync_failed`: Cloudflare **config GET** counters since the latest boot. `last_http_status` is the **Cloudflare config GET**, while `last_health_http_status` is the health POST.
- `last_config_error`: last GET error string and heap/RSSI context, or `none` after success. A negative HTTP code is a client/transport failure, not a server rejection such as 401 or 403.
- `tls_free_heap` and `tls_largest_heap_block` sample memory before a config attempt; `loop_stack_min_free_bytes` measures stack, not heap.
- `ota_stage.state=installed` is phase 3; `verified-boot` is phase 4 after an accepted cloud health POST. Cloudflare's server-side `verified-boot` audit can occur on phase 3, before a subsequent phase-4 health message.
- Cloudflare D1 `mesh_gateway_health.last_http_status` is the **telemetry uploader's** latest HTTP status forwarded in the health report, **not** the Cloudflare config GET HTTP status.
- Off site, use Cloudflare D1 for gateway heartbeats and **SE Utah Watersheds' `moab_watersheds.creek_observations`** for real Heltec-origin telemetry. A READY Vercel build alone does not establish successful uploads.
- A `received_at` timestamp proves a health check-in; repeated low uptime suggests resets but does not reveal why.

## Remote installation, rollback and safety

1. Verify **successful** V11 GitHub Actions build, application-only BIN SHA-256 and `SOURCE_COMMIT`.
2. Confirm a recent `loader_ready=1` heartbeat, the original Vercel telemetry path, and `ota_enabled=0` before approving a new release.
3. Register an approved GitHub BIN URL or upload to Cloudflare KV. **V10 can download only approved GitHub firmware URLs**. V11 adds KV support (`fw_kv=1`); newly written KV objects must propagate before use.
4. Explicitly approve the specific version. The Heltec validates the signed manifest, checks image SHA-256, stages after the **OTA_1 reserved 0xA0000 bytes**, and boots the independent CCA loader. The loader writes OTA_0, without factory erase or NVS changes.
5. A matching authenticated post-upgrade heartbeat allows the Worker to disable OTA and log a verified installation. Confirm phase 4, config sync and stable continued telemetry separately.
6. Keep the standalone `CCA_PULL_1` loader intact at OTA_1 offset `0x650000`. **No complete automatic rollback exists for a validly installed image that later crashes.** Onsite recovery can still be necessary.

**Crucial V10 distinction:** the historically field-installed V10 build 24 was SHA-256 `33551a8376cc3e391743c27b417932f28efc064a4a3d11ba24f2b39fd7ee87f3`. The **currently published** V10 branch BIN's `BUILD.txt` instead lists SHA-256 `45b4218ee591d142d1193918e16cc6e18e5343520761ed1c352e1573d46979fe`. They are **different artifacts**, despite related filenames. Do not blindly flash a replacement as an assumed tested rollback.

[V9 Clock](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary) remains the last firmware before Cloudflare and remote management, **not** an automatic rollback slot. [V10](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v10) is the previous gateway version.

## Source, detailed docs and regression checks

- [V11 Cloudflare client](src/modules/CcaCloudRemote.cpp), [recovery staging](src/modules/CcaRemoteOtaStage.cpp), [legacy telemetry routing](src/modules/HoboHttpGatewayModule.cpp).
- [Cloudflare Worker](cloudflare/cces-mesh-remote/worker.js) and [firmware manager admin runbook](cloudflare/cces-monitor/README-FIRMWARE-MANAGEMENT.md).
- [V10 routing/auto-DM preservation test](test/test_heltec_v10/test_cloud_remote.py), [V11 Cloudflare KV and diagnostic test](test/test_heltec_v11/test_cloudflare_kv.py).
- [V11 build workflow](.github/workflows/build_cca_heltec_gateway_v11.yml), [loader design and limitations](tools/cca_remote_ota/README.md).
