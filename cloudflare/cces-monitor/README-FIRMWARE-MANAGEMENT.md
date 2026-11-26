# CCES Monitor — Firmware Management
Deployed November 25, 2026 (Mountain Time).

## Live dashboard
https://cces-monitor.raspy-lake-a7f4.workers.dev/#firmware

The **Firmware** tab is an administrator-only section of the existing CCES
Monitor Worker. Existing Water monitoring, Lift stations, Pump diagnostics,
Cloudflare Access, and ingest operations are preserved. Access JWTs are
verified server-side. All non-GET API actions enforce the same-origin check.

## Cloudflare resources
- Dashboard Worker: `cces-monitor`
- Gateway/control Worker: `cces-mesh-remote`
- Existing database: D1 `cces-monitor`
- Firmware storage: KV `cces-heltec-firmware` (binding `FW_BIN` on both Workers)
- Gateway: `heltec-moab`
- Files are stored under `heltec-v10/<random-64-hex-token>` keys, served by
  opaque `/v10/firmware/<token>` URLs with `Content-Length` and no caching.
- R2 was not enabled on this Cloudflare account, so the pilot uses KV for
  modest, read-infrequent firmware images instead.

## Database
- `mesh_firmware_releases`: release ID, name, exact SHA-256, byte size, URL,
  backing store, actor and creation time.
- `mesh_firmware_actions`: administrator approvals, disable operations, uploads
  and gateway-verified installation events.
- Existing `mesh_gateway_config` drives five-minute signed manifest polls.
- `mesh_gateway_health` contains periodic OTA phase and loader-ready evidence.

## Administrator workflow
1. **Upload BIN** directly to KV, or register a GitHub raw BIN URL after the
   Worker independently downloads and verifies the declared SHA-256.
2. The release is recorded but **not automatically approved**.
3. Choose **Approve & install** and type `DEPLOY` when prompted. The server
   requires a recent authenticated loader-ready health report and refuses to
   approve a second release while the prior OTA switch is enabled.
4. Heltec retrieves the HMAC-signed manifest, downloads the SHA-pinned BIN,
   stages and verifies it away from the recovery loader, then reboots into the
   loader, which installs the new app. An authenticated matching phase 3/4
   heartbeat turns off the OTA authorization automatically.
5. The Firmware tab displays health reports and the action log. The **Disable
   remote OTA** control can turn off any pending release authorization.

## V10 to V11 transition
V10 supports remote downloads from the approved GitHub repository. V10 does
**not** permit Cloudflare firmware download URLs; the UI and server block KV
release approval until the gateway health note reports `fw_kv=1`.

V11 adds the Cloudflare URL allowlist and the `fw_kv=1` capability heartbeat.
Bootstrap V11 using an approved **GitHub** V11 release in the dashboard. Once V11
is installed and its health check proves this capability, directly uploaded KV
releases may be approved. Newly uploaded KV objects require a 2-minute
propagation period before approval and are rechecked for SHA-256 integrity.

## Safeguards and limitations
- Release names and GitHub hosts are validated. Raw uploads are limited to
  6 MB, must start with an ESP32 image header, and receive SHA-256 verification.
- The existing OTA_1 CCA recovery loader is not overwritten by normal OTA.
- The loader can retry an interrupted write, but an application that installs
  validly and **then crashes** has no automatic rollback. Recovery can still
  require local intervention.
- Existing V9 auto-DM and Vercel/Cloudflare sensor routing are unchanged.
- Gateway firmware must be tested on hardware before unattended production
  rollout. Remote installation is disabled by default until explicitly approved.

## Sources
- Deployed dashboard source: `cloudflare/cces-monitor/firmware-manager-2026-11-25.js`
- Gateway Worker: `cloudflare/cces-mesh-remote/worker.js`
- Heltec V11: `src/modules/CcaCloudRemote.cpp`,
  `src/modules/CcaRemoteOtaStage.cpp`
- Build: `.github/workflows/build_cca_heltec_gateway_v11.yml`
- Test: `test/test_heltec_v11/test_cloudflare_kv.py`
