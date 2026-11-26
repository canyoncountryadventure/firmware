# Heltec Gateway V11 • November 26, 2026

**Official dated release notes and operational verification for the Heltec V4 ESP32-S3 gateway.** This is an application firmware, **not a factory image or a RAK/Seeed build**.

## Release files and provenance

| File | Size / SHA-256 |
| --- | --- |
| [`Heltec-Gateway-v11-11-26-26.bin`](Heltec-Gateway-v11-11-26-26.bin) | **2,232,880 bytes**; `f3d7564a78e44fc2487982c0fa9dcfe267cd948ee08689fad956d724d66b66cc` |
| [`Heltec-Gateway-v11-11-26-26.zip`](Heltec-Gateway-v11-11-26-26.zip) | `458f56397447f27650beb67541778cf2b6e83ea9ae148694fc5ce7d0c0a82a02` |
| [`BUILD.txt`](BUILD.txt) | Full checksum manifest, source SHA, CI link |

- Build hardware: Heltec WiFi LoRa 32 V4 OLED; PlatformIO `heltec-v4`, ESP32-S3.
- Build source commit: **`6bd896c3a557846325492296f55b643286d17d12`**.
- CI: [run 45866228411](https://github.com/canyoncountryadventure/firmware/actions/runs/45866228411), **all three jobs successful** (compatibility tests, application compilation, publication).
- The source commit in `BUILD.txt` identifies the compiled code. Later docs-only commits do not change the BIN.
- Verify SHA-256 before any manual or remote flash. Do not use the ZIP as an OTA BIN and never use a factory/erase image on an existing configured gateway.

## Precisely what changed in V11

1. **Cloudflare TLS retry behavior**: the ordinary config GET stays on a five-minute schedule after a success; after failed requests, retry delays are **30, 60, 120, 240 seconds**, capped at 240 seconds. Client socket/connect/HTTP timeouts are **12 seconds** rather than V10's 10-second HTTP timeout. This improves recovery from intermittent failures but does not prove their cause.
2. **Local Cloudflare diagnostics**: added `last_config_error` (HTTPClient error and pre-request heap/RSSI context), `last_health_http_status`, `tls_free_heap`, `tls_largest_heap_block` and `tls_wifi_rssi_dbm` to the gateway's existing `cloudflare` JSON. The `last_http_status` field still concerns Cloudflare's config GET, not Vercel.
3. **Cloudflare firmware library support**: accepts SHA-pinned firmware from the approved GitHub repository or an opaque 64-character lower-case hex download URL on the dedicated Cloudflare Worker. The gateway exposes `fw_kv=1` in its OTA health note.
4. **Preserved prior functionality**: V9 automatic missing-report DMs, station interval controls, manual READ, BLE HOBO sampling, existing Vercel/Neon uploads, gateway NVS, radio routing and the two Cloudflare-only ultrasonic station IDs are unchanged. The same independent CCA OTA loader is reused, not reflashed.

## Real field rollout evidence

- **V10 before upgrade**: `HELTEC-GATEWAY-V10` with Cloudflare config V6 and `ota_enabled=0`. Recovery-loader health reported `loader_ready=1`.
- **November 26, 2026, 10:28:31 AM MST**: published V11 BIN's exact SHA/URL approved in Cloudflare config **V7**.
- **10:36:23 AM MST**: authenticated health report:
  ~~~text
  firmware=HELTEC-GATEWAY-V11
  config_version=7
  uptime_seconds=32
  remote-ota: loader_ready=1,phase=3,attempts=0,applied=heltec-v11-cloudflare-tls-20261126,fw_kv=1
  ~~~
- Cloudflare automatically wrote config **V8 with OTA disabled** and recorded action `verified-boot`.
- SE Utah Watersheds continued receiving Heltec-origin telemetry after the upgrade. This verifies one live boot and upload continuity; it **does not establish that the client later reported phase 4 or that uptime remained stable for days**.
- Installer `attempts=0` is the recorded stage retry counter, not proof that there were no network failures during polling.

## Station routing and calibration

| Station | ID | Backend |
| --- | --- | --- |
| DYPA08 1 | `!55ca0677`, `1439303287` | Cloudflare D1 only |
| DYPA08 2 | `!636eb002`, `1668198402` | Cloudflare D1 only |
| Original watershed, HOBO and relay stations | Existing allowlist | **SE Utah Watersheds / Vercel/Neon**, unchanged |

Cloudflare DYP-A08 raw `distance_mm` is **not** a calibrated stream level. Without a verified reference stage, `stage_mm` must remain unavailable. The Heltec firmware update does not change RAK4631 node firmware, its sample intervals, radio keys or sensor calibration.

## Quick read-only diagnostic

On the gateway LAN or VPN:

~~~powershell
$c = (Invoke-RestMethod "http://192.168.1.147/json/gateway").cloudflare
$c | Select-Object remote_config_version,sync_ok,sync_failed,last_http_status,last_config_error,last_health_http_status,tls_free_heap,tls_largest_heap_block,tls_wifi_rssi_dbm,loop_stack_min_free_bytes,ota_stage
~~~

The counters reset after a restart. `last_config_error` is reset to `none` after a successfully parsed config. The remote `/v10/health` report only includes the coarse OTA note and **telemetry uploader's** HTTP status; the detailed GET/TLS diagnostics are in the local gateway JSON, not the D1 health table.

## OTA process, what works and what doesn't

1. Approve a **specific** checked release in CCES Monitor / Firmware. The firmware obtains the approved manifest from **`/v10/config`**, even in V11 (old route name is intentional).
2. HMAC-SHA256 authenticate the OTA manifest, validate URL host/token and binary SHA-256.
3. Write verified staged bytes into OTA_1 **after** the first `0xA0000` bytes reserved for the independent `CCA_PULL_1` loader. The OTA_1 base is `0x650000` on this Heltec V4 partition map.
4. Reboot into the independent loader, which writes the image into OTA_0 and boots the application.
5. An authenticated matching phase-3/4 health event allows the Worker to turn OTA **OFF**. The client marks phase 4 after a successful health POST.
6. Verify later heartbeats, config adoption and SE Utah Watersheds telemetry; no OTA update is considered permanently stable based only on phase 3.

**Limits and caveats:** There is **no full rollback** for a flash-valid firmware that later crashes after boot; a local repair could be needed. TLS certificate identity checking is still disabled by `setInsecure()`; the OTA manifest uses separate HMAC integrity, not a replacement for general TLS trust. This build does not definitively identify or eliminate the historical `HTTP -1` root cause.

## Upgrade and rollback references

- [Main V11 readme with full architecture and troubleshooting](../../README.md)
- [Cloudflare firmware-management runbook](../../cloudflare/cces-monitor/README-FIRMWARE-MANAGEMENT.md)
- [Recovery-loader design](../../tools/cca_remote_ota/README.md)
- [V10 predecessor](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v10), [V9 Clock last pre-Cloudflare](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary)

**Do not confuse historical V10 build 24** (observed SHA-256 `33551a8376cc3e391743c27b417932f28efc064a4a3d11ba24f2b39fd7ee87f3`) with the later V10 branch's released BIN (SHA-256 `45b4218ee591d142d1193918e16cc6e18e5343520761ed1c352e1573d46979fe`). The files are different. Always choose a rollback by verified hash rather than just filename.
