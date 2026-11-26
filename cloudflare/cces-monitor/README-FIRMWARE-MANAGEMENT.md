# CCES Monitor | Cloudflare Firmware Management

**Operations runbook • November 26, 2026 • Heltec Moab**

## Verified production state

- V11 firmware release `heltec-v11-cloudflare-tls-20261126` remotely installed on November 26, 2026 at **10:36:23 AM MST**; authenticated first V11 heartbeat reported `phase=3`, `attempts=0`, `loader_ready=1` and `fw_kv=1`.
- The Worker recorded `verified-boot`, advanced D1 `mesh_gateway_config.version` from **7 to 8**, and set `ota_enabled=0` in the same transaction as installation acknowledgment.
- **Do not equate the server's audit action with a later phase-4 heartbeat or long-duration stability.** Confirm subsequent `phase=4`, config V8 adoption, cloud sync reliability and telemetry.
- SE Utah Watersheds (Vercel/Neon) remained independently operational after the first V11 boot.
- This deployment did not flash or reconfigure any RAK sensor.

## URLs and account resources

| Service | Resource |
| --- | --- |
| Owner dashboard | https://cces-monitor.raspy-lake-a7f4.workers.dev/#firmware |
| Dashboard Worker | `cces-monitor`, existing Cloudflare Access administrator authentication |
| Gateway Worker | `cces-mesh-remote`, https://cces-mesh-remote.raspy-lake-a7f4.workers.dev |
| D1 | `cces-monitor` (ID `850bb203-caac-4695-bcc9-7ac10f97ad49`) |
| KV | `cces-heltec-firmware`, bound to both Workers as `FW_BIN` |
| Gateway | `heltec-moab` |
| Separate watershed backend | Vercel `seutahwatersheds`, Neon `moab_watersheds` |

KV objects are stored under `heltec-v10/<64-hex-token>` **for route compatibility** even with V11. Files download from `GET /v10/firmware/<token>`. The URL token is random and unguessable; possession of an actual URL allows its download, so **do not treat that endpoint as a password-authenticated private file service**. The signed release manifest and SHA-256 check authorize firmware installation. R2 is not enabled; this pilot uses KV and is designed for infrequent firmware images, not general file hosting.

## Admin page and permission boundaries

The Firmware tab is an admin-only addition to the original CCES Monitor Worker, preserving existing water monitoring, lift stations, pump diagnostics, Access controls and D1 secrets. Server-side role checks apply to every `/api/firmware` endpoint; mutation requests also enforce same origin.

- **GET /api/firmware**: current gateway approval/config, recent health, release library, audit history.
- **POST /api/firmware/upload**: raw ESP32 application BIN, version in header. Size **4 KB–6 MB**, first byte `0xE9`, SHA-256 computed and stored. This checks container type and integrity, **not** full hardware compatibility; CI must still verify the image.
- **POST /api/firmware/register**: register a raw `raw.githubusercontent.com/canyoncountryadventure/firmware/` BIN only after the backend downloads it and verifies the exact provided SHA-256 and size.
- **POST /api/firmware/action**: `deploy` requires operator's typed `DEPLOY` confirmation and fresh loader-ready evidence; `disable` switches pending authorization off.
- **Firmware release library**: shows SHA, size, store, created time and approval controls. V10 only understands GitHub download hosts; KV approvals require a recent V11 gateway heartbeat containing `fw_kv=1`.
- **Actions log**: approval, upload, registration, disable and authenticated matching verified-boot events. This is an audit trail, not a replacement for actual gateway health.

Tables: `mesh_firmware_releases`, `mesh_firmware_actions`; existing `mesh_gateway_config`, `mesh_gateway_health`, `mesh_stations` and `mesh_gateway_dm_overrides`.

## Operational procedure: publishing and deploying

1. Build Heltec V4 ESP32-S3 **application** firmware in GitHub Actions. Verify every job succeeds and capture **`BUILD.txt`, exact source commit, BIN size, SHA-256 and raw URL**. Reject factory/erase images or a stale compiled source. The known-good V11 release BIN is `Heltec-Gateway-v11-11-26-26.bin`, SHA-256 `f3d7564a78e44fc2487982c0fa9dcfe267cd948ee08689fad956d724d66b66cc`, size 2,232,880 bytes.
2. Confirm OTA is **off**, the latest gateway health is recent and reports `loader_ready=1`, and recent SE Utah Watersheds records show ongoing Vercel uploads.
3. Register the checked GitHub raw BIN or upload the firmware to KV. **A KV upload does not remotely install itself.** Wait for KV propagation (the UI/server require 2 minutes before eligible approval), and require `fw_kv=1` from the gateway.
4. Explicitly authorize one release in the admin UI; operator types `DEPLOY`. The server only enables OTA if the previous OTA switch is off and a recent recovery loader heartbeat exists.
5. Gateway polls `GET /v10/config` (the `v10` route name is retained in V11), checks the signed manifest and SHA-256, stages the BIN **past the reserved OTA_1 loader first 0xA0000 bytes**, then boots the independent loader.
6. The loader writes OTA_0; a matching authenticated post-boot report causes the Worker to log `verified-boot` and disable OTA automatically. Verify the gateway eventually syncs the new config version, reports phase 4, and resumes live Vercel and Cloudflare telemetry.
7. Do **not** repeatedly authorize the same release. If the new app boots but crashes later, this scheme has **no automatic rollback image** and may require onsite access. Keep the independent `CCA_PULL_1` loader at OTA_1 base `0x650000` intact.

## Distinguishing gateway status from upload health

| Evidence | What it proves |
| --- | --- |
| Latest `mesh_gateway_health.received_at` | A valid authenticated gateway health POST arrived at that server time |
| `firmware=HELTEC-GATEWAY-V11` | App identified as V11; not proof of long-term stable running |
| `phase=3` | The installation completed and new app booted |
| `phase=4` | App previously received positive cloud health acknowledgment |
| `loader_ready=1` | Installed independent CCA loader recognized |
| `fw_kv=1` | V11 Cloudflare KV firmware host support compiled |
| `mesh_gateway_config.version=8,ota_enabled=0` | Server authorization disabled; client may take another config poll to adopt V8 |
| D1 health row `last_http_status` | **Vercel/telemetry uploader HTTP code**, not Cloudflare config GET |
| Local `cloudflare.last_config_error` | **Actual Cloudflare config GET error details**; only accessible on LAN/VPN |
| Latest `moab_watersheds.creek_observations` | Real SE Utah Watersheds data pipeline activity; inspect its raw radio.gateway=Heltec Hub to attribute to this gateway |

A failure marked `HTTP -1` means no valid HTTP response, **not** a proved Cloudflare 401, 403 or Worker fault. V11 retries and logs richer local context; the network cause still must be measured.

## Security, configuration and recovery caveats

- The Worker expects the shared `X-Ingest-Key` matching its `GATEWAY_KEY` binding, and HMAC-SHA256 signs firmware manifests. Keep keys out of GitHub and chat logs.
- The Heltec still calls `setInsecure()` for HTTPS, meaning it **does not validate the server's TLS certificate identity**. The HMAC protects OTA release metadata, but ordinary config is not equivalently signed. This is future hardening work.
- The firmware library's 64-hex-token URL is a bearer capability, not proof that the firmware itself is safe. The operator must still verify version/target/CI/checksum.
- Keep Vercel/Neon station routing separate from the DYP-A08 Cloudflare pilot. Gateway V11 does not reroute old sensors or change the RAK radio firmware.
- No factory erase, loader partition overwrite, or unverified autonomous rollback.
- D1 config and Worker UI can change remotely; a code change to the Heltec's HTTPS/TLS client **requires another carefully approved firmware OTA**.

## Reproducibility and source

- [Main firmware README](../../README.md)
- [V11 dated release README](../../downloads/Heltec-Gateway-v11/README.md), [BUILD.txt](../../downloads/Heltec-Gateway-v11/BUILD.txt)
- [Actual dashboard Worker source snapshot](firmware-manager-2026-11-25.js)
- [Gateway Worker source](../cces-mesh-remote/worker.js)
- [Client config/TLS code](../../src/modules/CcaCloudRemote.cpp)
- [Recovery installer](../../src/modules/CcaRemoteOtaStage.cpp)
- [Build CI workflow](../../.github/workflows/build_cca_heltec_gateway_v11.yml)
