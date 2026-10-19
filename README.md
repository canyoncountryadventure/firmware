# CCA Heltec Sensor Gateway V6

## October 19, 2026: verified CC01 ingestion repair

**Published and verified:** GitHub Actions compiled the Heltec V4 OLED application, passed the native parser/poller and V5 wire-compatibility tests, and published the October 19 BIN/ZIP. Both downloads were independently retrieved and their SHA-256 hashes checked. The BIN contains the independent-reply fix and `/json/gateway` route; the ZIP application image matches the standalone BIN exactly. Source: `2494b6206420b330b6cc317bdc59c300510d75e3`. [Successful build log and publication](https://github.com/canyoncountryadventure/firmware/actions/runs/39279737963). Live RF-to-Neon verification is still required after flashing.

The previous receive handler silently ignored valid CC01 sensor replies unless the poller was waiting for that exact response type. This could discard manual responses, late replies after a two-minute timeout, or replies arriving out of order. V6 now parses complete, authorized CC01 `WATER READ` and `MX2001` replies in every polling state and queues them through the existing HTTPS uploader. Only a matching response advances the scheduled poll; unrelated replies do not cancel or restart it. Automatic private packets and the 30-minute polling schedule are unchanged.

**Wi-Fi-only diagnosis:** open `http://192.168.1.147/json/gateway` (use the gateway's current IP) or run `Invoke-RestMethod http://192.168.1.147/json/gateway | ConvertTo-Json -Depth 5`. This read-only status endpoint reports the V6 fix identifier, uptime, Wi-Fi, whether an ingest credential exists (never its value), queue usage, total accepted uploads, and CC01 requests/replies/parsed/queued/rejected counts, last reply packet ID and last HTTP result. `cc01.uploaded > 0` with HTTP 2xx confirms the cloud accepted a DM reading; confirm its presence in Neon separately. Counters reset on reboot. No USB or serial monitoring is required.

Regression coverage replays the actual production poller with the October 19 screenshot replies in every state, including late replies, full queues, invalid readings and the existing startup/timeout schedule. The packet capture proved that a valid encrypted CC01 reply reached Heltec; it did not expose the poll state or HTTP result, so the timer gate is a confirmed discard path rather than a proven explanation for every missing historical packet.


**Updated October 19, 2026 | Heltec WiFi LoRa 32 hardware V4 OLED | ESP32-S3 | PlatformIO `heltec-v4`**

Firmware V6 includes scheduled CC01 polling and the encrypted-DM receive fix. The initial V6 build sent scheduled requests but rejected PKI-encrypted CC01 responses because Meshtastic decodes those DMs on local channel 0, not LongFast channel 1. V6 accepts an addressed reply only from CC01 on authenticated PKI channel 0 or legacy LongFast channel 1, adds parser-failure diagnostics, and no longer calls an unconfirmed send attempt 'queued'. Existing automatic DS/MX packets, all other station handlers, calibration, Wi-Fi, BLE, watchdog, OTA, and the RAK firmware remain unchanged.

## Verified V6 downloads and build provenance

- [**Download Heltec V6 application BIN, October 19, 2026**](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v6/downloads/Heltec-Gateway-v6/Heltec-Gateway-v6-10-19-26.bin)
- [**Download Heltec V6 OTA/full-flash ZIP, October 19, 2026**](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v6/downloads/Heltec-Gateway-v6/Heltec-Gateway-v6-10-19-26.zip)
- [**Source commit and SHA-256 checksums**](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v6/downloads/Heltec-Gateway-v6/BUILD.txt)
- [**Build and regression-test logs**](https://github.com/canyoncountryadventure/firmware/actions/runs/39279737963)
- [Rollback: Heltec V5 (automatic telemetry only)](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v5)
- [RAK field firmware V6 (unchanged, separate UF2)](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v6)

| Published file | SHA-256 |
| --- | --- |
| `Heltec-Gateway-v6-10-19-26.bin` | `def468f75664eec2825468d79ef97d3190ef5ca14298302ab8903b8ad0640bcf` |
| `Heltec-Gateway-v6-10-19-26.zip` | `fc98ec73cd5ea5fe3a5f79f4b814eaa0c4515e3ea5b8037cbf5b67fc335f648b` |

Embedded firmware version: `2.7.26.2494b62`. Source commit: `2494b6206420b330b6cc317bdc59c300510d75e3`. The V6 branch includes changes previously labeled V7; no separate V7 branch is needed.

**This is Heltec hardware V4 firmware, not a RAK UF2.** The standalone BIN is the application image for routine Wi-Fi Unified OTA. Preserve NVS/configuration and the existing OTA loader. The ZIP also contains a factory image, which is not the routine Wi-Fi update target.

### Wi-Fi flash from PowerShell

Close any TCP monitor first. Download and verify the current application, then use Meshtastic's Wi-Fi updater:

```powershell
$ErrorActionPreference = "Stop"
$firmwarePath = Join-Path $PWD "Heltec-Gateway-v6-10-19-26.bin"
Invoke-WebRequest -UseBasicParsing "https://raw.githubusercontent.com/canyoncountryadventure/firmware/4130004bbeafe9c84b8aafd9068a929d29f8c984/downloads/Heltec-Gateway-v6/Heltec-Gateway-v6-10-19-26.bin" -OutFile $firmwarePath
if ((Get-FileHash $firmwarePath -Algorithm SHA256).Hash -ne "def468f75664eec2825468d79ef97d3190ef5ca14298302ab8903b8ad0640bcf") { throw "Firmware checksum mismatch" }
$ErrorActionPreference = "Continue"
python -m meshtastic --host 192.168.1.147 --ota-update $firmwarePath
if ($LASTEXITCODE -ne 0) { throw "Wi-Fi updater failed; inspect its output" }
```

After Heltec reconnects, inspect `http://192.168.1.147/json/gateway`. Use the actual current IP if it changed. A manual `READ` sent from Heltec to `!65165646` can now be ingested immediately. Verify `cc01.uploaded` increases and the new reading is present in Neon with `source: "heltec_dm"`. Scheduled first-poll delay and 30-minute interval are unchanged.

## Encrypted-DM receive repair included in V6

On Meshtastic 2.7.26, a PKI-encrypted direct message is decrypted for the destination with **local channel index 0**, regardless of the LongFast index chosen when composing the request. The initial V6 `wantPacket()` accepted CC01 `TEXT_MESSAGE_APP` replies only if `channel == 1`. Valid encrypted replies were dropped before `handleCc01Reply()`. V6 fixes the receive filter while preserving sender/destination authorization and the existing LongFast fallback.

V6 also reports outbound requests as **attempted, delivery unconfirmed** rather than claiming the RF network queued/delivered them, and logs when a CC01 reply arrives but does not match the strict `WATER READ` or `MX2001` parser. The 2-minute initial delay, 30-minute polling, reply timeouts, and unconditional automatic packet handling are unchanged.

**On-device validation still required:** reboot V6 on Heltec with Wi-Fi online, observe `CCA CC01 DM poll: sent request WATER READ`, then verify at least one decoded CC01 DM reply is uploaded with `metrics.source = "heltec_dm"` in `moab_watersheds.creek_observations`. This distinguishes sending, receiving/parsing and HTTPS storage. Compilation alone does not prove LoRa delivery.

## Scheduled DM behavior

Every **30 minutes**, Heltec sends two separate Meshtastic direct messages to **CC01 / Cottonwood Creek `!65165646`** over the gateway's local **LongFast channel 1**:

1. **`WATER READ`**: requests a fresh MB7388 ultrasonic reading. Only a complete, calibrated `WATER READ` reply is converted to a normal `water_distance` observation with raw distance (mm) and stage (ft/mm).
2. **`READ`**: requests a fresh HOBO MX2001 BLE read on CC01. A complete `MX2001` reply becomes an `mx2001` observation with logger MAC, water level, temperature F/C and BLE RSSI. Text has two-decimal-foot and one-decimal-F precision because that is what the **RAK V6 READ DM currently returns**. No extra precision is fabricated.

The first cycle starts around **2 minutes after Heltec boots**; following cycles start every 30 minutes. Heltec waits up to **2 minutes** for the WATER reply, then sends READ after a short spacing delay and waits up to 2 minutes for that reply. A timeout leaves the missing measurement **unsaved**, logs a warning, and does not block future cycles. Unrelated text DMs cannot produce readings. No request is considered delivered merely because Meshtastic queued it.

Both successful parsed replies use the **existing V5 HTTPS upload queue**: **Heltec → Vercel `/api/ingest` → Neon `moab_watersheds.creek_observations` → watershed dashboard**. Uploads have CC01's node ID and the original Meshtastic DM reply packet ID. The payload marks the provenance as `source: "heltec_dm"`; unavailable logger raw values and sequence numbers are omitted rather than invented. Auto packets retain their original fields. If both routes report the same physical reading, **both are stored** as requested.

### Automatic transmissions remain enabled

**No changes whatsoever** to the V5 inbound `DS` (`PRIVATE_APP`) water handler, `MX` HOBO handler, standard environmental/device telemetry, other stations, gateway local BLE HOBO reader, mesh forwarding, Wi-Fi HTTPS, OTA, watchdog or resets. CC01 can still report automatically to Heltec at any time. **No `HUB SET`, `HUB AUTO`, sensor interval, or RAK firmware change** is required.

Once CC01's automatic packets are reliably arriving, the safest tested rollback is to flash the dated **Heltec V5** application over Wi-Fi, preserving configuration, and resume auto-only mode. V6 does not silently disable the field node's automatic telemetry.

## Legacy capabilities preserved from V5

| Sensor/source | Existing protocol | Gateway behavior |
| --- | --- | --- |
| Remote HOBO MX2001 (old) | `MX` V1 19-byte private packet, tenths | Existing accurate legacy decoding |
| Remote HOBO MX2001 (RAK V6) | `MX` V2 21-byte private packet, hundredths | Existing signed stage and hundredths decoding |
| Direct local HOBO BLE | MX2001, MX2201, MX2203 | Existing `LOGGER`, `READ`, `LOCK`, `UNLOCK`, BLE lock and auto scanner |
| Ultrasonic water distance/stage | `DS` 24-byte private packet | Existing calibrated distance and stage upload |
| Moisture/PIR/soil | `RK` / `SM` private packets | Existing unchanged payloads |
| Temperature and battery | Standard Meshtastic telemetry | Existing voltage, battery percentage, environmental temperature, uptime |

All accepted sensor data is still uploaded independently with the existing V5/V6 ingest credential. Local watchdog, diagnostics (`DIAG`, `CRASHLOG`, `WATCHDOG`), SX1262 recovery and 12-hour maintenance reboot are retained.

## Wi-Fi OTA and rollback

1. Download the **dated V6 OTA/flash ZIP** from the release directory above. Confirm BUILD.txt source commit, board target and SHA-256.
2. Heltec's previously observed Wi-Fi address was `192.168.1.147`; verify it is still current. Meshtastic TCP API: **4403**. ESP32 Unified OTA loader: **3232**.
3. Use the Meshtastic Unified OTA updater with the correct `heltec-v4` ESP32-S3 firmware image. **Preserve NVS/config**; do not factory erase. Keep power and Wi-Fi stable throughout.
4. Verify Heltec rejoins the mesh and uploads a normal automatic sensor packet. Wait 2 minutes, then observe `CCA CC01 DM poll:` logs or, more reliably, watch Neon for `water_distance`/`mx2001` with `raw.payload.source = "heltec_dm"`.
5. To return to auto-only Heltec, install the dated V5 OTA image the same way. This changes gateway firmware only and does not touch CC01.

**Cloud verification:** Observe at least one complete CC01 DM reply stored in Neon, and separately verify that standard Pack Creek and CC01 auto `PRIVATE_APP` packets still pass. Compilation/tests do not prove field RF delivery or live HTTPS acceptance.

## Build and QA

The dedicated [V6 dated firmware workflow](.github/workflows/build_cca_heltec_gateway.yml) compiles only **`heltec-v4`** using the GitHub Actions `HOBO_HTTP_GATEWAY_INGEST_KEY` secret, runs V5 protocol compatibility tests and V6 native DM-parser and channel-routing tests, then publishes an actual date-named BIN, ZIP, `BUILD.txt`, SHA-256 checksums and exact source commit. Publishing is blocked if the branch has advanced since the build.

See [Gateway V6 operations and non-regression checklist](docs/CCA_HELTEC_SENSOR_GATEWAY.md). V5 remains available for auto-only rollback.
