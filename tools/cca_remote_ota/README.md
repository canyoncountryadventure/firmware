# Heltec V10 standalone self-update loader

This is an overlay for the official Meshtastic unified ESP32 OTA loader.
The loader remains in the existing **OTA_1 partition at 0x650000** on
Heltec V4 16 MiB devices; no repartitioning or NVS erase.

**Why two programs?** Meshtastic V10 runs from OTA_0. Its existing OTA_1
loader takes over during firmware replacement. An independent updater cannot
run concurrently with the active application. We preserve the loader, stage
the approved BIN in the UNUSED tail of OTA_1, then let the loader copy the
verified staged image into OTA_0.

## Data and ownership

- Config/policy: Cloudflare D1 mesh_gateway_config row heltec-moab
- Signed manifest fetched by V10 from GET /v10/config every five minutes
- Source BIN: approved GitHub HTTPS URL (SHA-256 pinned by signed manifest)
- V10 staging offset inside OTA_1: 0xA0000, loader first 640 KiB protected
- ESP-IDF NVS namespace: cca_remote_ota
- Approved loader app descriptor version: CCA_PULL_1
- Loader recovery: native port 3232 and BLE OTA remain available
- Cloudflare OTA switch defaults OFF; no unattended install before bench test

The loader must be installed **once through USB** at the OTA_1 offset.
DO NOT use a factory erase; DO NOT flash this loader at address 0.
Do not overwrite the main OTA_0 firmware, NVS, Meshtastic channel keys,
radio identity or the LittleFS partition.

Build workflow pins official Meshtastic ESP32 unified OTA commit
`6258997041a3b6c06b7581bb91421000025d1f2b` (includes Oct 2026
stack overflow remediation), overlays this minimal CCA stage reader and
enforces a 640 KiB maximum loader BIN. The binary is not approved for
deployment until tested on a spare Heltec V4 with the real layout.

### Runtime sequence

1. Cloudflare publishes an approved HMAC-signed release.
2. V10 validates signature, checks matching loader descriptor, downloads
   to OTA_1 tail and verifies SHA-256 both in transit and from flash.
3. V10 stores metadata in NVS and boots the independent loader.
4. Loader rechecks the staged hash, writes OTA_0, retries failures (max 10).
5. Loader selects OTA_0 only when the image passed esp_ota_end validation.
6. New V10 boots and reports health. An interrupted write leaves OTA_1
   bootable to retry; after failures it remains in OTA_1 for local repair.

**Recovery limitation:** A firmware that passes image validation but later
crashes after boot is NOT automatically rolled back by this design. The prior
OTA_0 application image is overwritten. That failure mode needs a tested
boot-time fallback/backup policy before unattended production use. This
repository intentionally does not claim autonomous rollback or complete
remote-recovery protection.

**No remote RAK/Seeed firmware updates:** this covers the Heltec gateway.
