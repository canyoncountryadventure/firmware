# Heltec Gateway V10 | Cloudflare + Remote Management

**Historical V10 gateway firmware • November 25, 2026 • Heltec WiFi LoRa 32 V4 / ESP32-S3**

**Status:** V10 is the **previous** Heltec gateway generation. The actual gateway remotely upgraded to [V11 on November 26, 2026](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v11). V9 Clock is the **last firmware before Cloudflare and remote configuration**; V10 added the Cloudflare D1 pilot and the tested CCA remote-upgrade path. Do not confuse historical claims from early V10 experiments with the later confirmed V10/V11 field installations.

## Firmware downloads, provenance and critical hash warning

- [V10 application-only BIN](downloads/Heltec-Gateway-v10/Heltec-Gateway-v10-11-25-26.bin)
- [V10 ZIP](downloads/Heltec-Gateway-v10/Heltec-Gateway-v10-11-25-26.zip)
- [**V10 release README**](downloads/Heltec-Gateway-v10/README.md)
- [`BUILD.txt` with SHA-256, build run and source SHA](downloads/Heltec-Gateway-v10/BUILD.txt)
- [V10 Build 45762097828](https://github.com/canyoncountryadventure/firmware/actions/runs/45762097828)
- [V11 release and updated troubleshooting](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v11/downloads/Heltec-Gateway-v11)
- [V9 Clock, last verified pre-Cloudflare](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v9-hour-boundary)

The **currently published** V10 file `Heltec-Gateway-v10-11-25-26.bin` is a 2,231,856-byte application image with SHA-256:

`45b4218ee591d142d1193918e16cc6e18e5343520761ed1c352e1573d46979fe`

`BUILD.txt` lists compiled source commit `b666ce18207acb2a3d391e5c170a3ea47b7a099f`. Use an image verified by the matching SHA and build, **never the factory image or a guessed binary**.

**Important: a different V10 build 24 was actually installed and remote-tested** before V11, release name `heltec-v10-downloaderfix-cloud-test-b24`, SHA-256:

`33551a8376cc3e391743c27b417932f28efc064a4a3d11ba24f2b39fd7ee87f3`

Those two hashes **do not match**, despite similar filenames/source branches. A rollback to the historically installed build 24 requires an independently verified copy of that artifact; simply downloading this branch's current dated BIN is **not** an exact build-24 rollback.

## How V10 differs from V9 Clock

| Feature | V9 Clock | V10 |
| --- | --- | --- |
| Legacy Vercel/Neon station routing | Yes | **Preserved** |
| V9 missing-automatic/clock DMs, manual READ and saved intervals | Yes | **Preserved** |
| HOBO BLE and standard station telemetry | Yes | **Preserved** |
| New Cloudflare-only ultrasonic nodes | Not in V9 | **DYPA08 1, DYPA08 2** |
| Cloudflare D1 remote config | None | Validated, cached and polled every five minutes |
| Remote DM overrides | Local/DM | Optional overrides for existing watched stations |
| Remote health | Local JSON | Additional Cloudflare health POST nominally every 15 minutes |
| Cloudflare release approval | None | HMAC-SHA256 OTA manifest and pinned firmware SHA |
| Unattended OTA application install | Not part of V9 Clock | Supported on the later V10 + separately installed `CCA_PULL_1` loader |
| Cloudflare KV-hosted firmware downloads | Not supported | Not supported until V11 |

## Sensor identities and calibration

- `DYPA08 1`: node ID `!55ca0677`, node number `1439303287`.
- `DYPA08 2`: node ID `!636eb002`, node number `1668198402`.

Only these Cloudflare pilot nodes are forwarded through the Cloudflare-only route in the original V10 configuration; all pre-existing monitored station traffic remains on the Vercel/Neon path. The DYP RAK4631 sensors broadcast 24-byte `DS` distance packets. **Uncalibrated distance is not water stage.** Stage must remain NULL/unknown until a real reference is recorded. Updating Heltec V10 never reflashes the RAK radios.

## Cloudflare gateway architecture

Dedicated Worker `cces-mesh-remote` at `https://cces-mesh-remote.raspy-lake-a7f4.workers.dev`. D1 database `cces-monitor` contains `mesh_stations`, `mesh_gateway_config`, `mesh_gateway_dm_overrides`, `mesh_gateway_health`, `mesh_observations` and `mesh_device_telemetry`.

- `GET /v10/config`: gateway-initiated config pull, normally every five minutes. Validates station destinations, versions and optional DM overrides; cached settings preserve remote station IDs during Wi-Fi outages.
- `POST /v10/ingest`: distance/device telemetry from Cloudflare-assigned nodes.
- `POST /v10/health`: periodic gateway health status, plus OTA loader readiness/installation phase.
- `GET /healthz`: Worker-only availability check, **not** evidence Heltec connected.

The Cloudflare Worker needs its `GATEWAY_KEY` secret matching the Heltec build secret `HOBO_HTTP_GATEWAY_INGEST_KEY`. Never commit either. The gateway initiates outbound requests, so no public router port is needed.

Optional DM overrides in `mesh_gateway_dm_overrides` apply only to supported V9 watched nodes. Allowed timeout values are **15, 30, 45, 60, 360, 720, 1440 minutes**. Existing local DM settings, node allowlist, scheduler and safety safeguards remain. A remote configuration version increment makes updated settings eligible at the next successful poll.

## What was verified in the field

1. V10 with the companion recovery loader `CCA_PULL_1` installed successfully performed a Cloudflare-triggered V10 application OTA in November 2026. The local OTA status reached `verified-boot` and cloud configuration was disabled after the installation.
2. Cloudflare later showed intermittent configuration GET errors including `HTTP -1`, with eight failed vs four successful GET attempts on one reboot. This indicates an HTTPS client/transport problem; it does **not** prove Cloudflare returned a 401/403/500.
3. At the same time, actual Heltec-origin packets still arrived through **SE Utah Watersheds Vercel/Neon**, independently of the Cloudflare pilot.
4. [V11](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v11) was built, remotely installed and first-boot acknowledged on **November 26, 2026**, extending host support and improving TLS diagnostics and retry intervals. This is the current gateway generation.

## Remote OTA, recovery and safety

The V10 gateway validates an explicit HMAC-signed release manifest, approved GitHub firmware host and SHA-256, then stages an application image into OTA_1 after the first **`0xA0000`** bytes used by the independent loader. The loader occupies OTA_1 at base `0x650000`, reads the verified staged firmware and writes OTA_0. It has bounded retry behavior; do **not** erase flash, NVS, keys, loader or the partition table during normal updates.

**No full automatic rollback exists for a validly flashed application that later crashes.** Physical recovery may still be required. The loader supports local recovery separately, but that is not a guarantee of unattended fault recovery.

Transport uses `WiFiClientSecure::setInsecure()` and does not validate the TLS server certificate; OTA manifest HMAC and image SHA-256 provide separate release integrity checks. Ordinary remote node config is not protected by that OTA HMAC.

## Read-only gateway troubleshooting

On the Heltec LAN or VPN:

~~~powershell
$c = (Invoke-RestMethod "http://192.168.1.147/json/gateway").cloudflare
$c | Select-Object remote_config_version,sync_ok,sync_failed,last_http_status,ota_stage,loop_stack_min_free_bytes
~~~

V10 does **not** expose V11's additional `last_config_error`, `last_health_http_status` or heap/RSSI diagnostic fields. `sync_ok` and `sync_failed` reset on reboot. Server `mesh_gateway_health.last_http_status` reflects the **telemetry uploader's** latest HTTP status, not the config GET. Confirm actual SE Utah Watersheds uploads from its `moab_watersheds.creek_observations` table rather than by the unrelated main Meshtastic website.

## Related source

- [V10 Cloudflare client](src/modules/CcaCloudRemote.cpp)
- [V10 OTA staging and loader protections](src/modules/CcaRemoteOtaStage.cpp)
- [V10 original telemetry router](src/modules/HoboHttpGatewayModule.cpp)
- [V10 worker](cloudflare/cces-mesh-remote/worker.js)
- [V10 routing/OTA regression tests](test/test_heltec_v10/test_cloud_remote.py)
- [V10 build workflow](.github/workflows/build_cca_heltec_gateway_v10.yml)
- [CCA loader design](tools/cca_remote_ota/README.md)
