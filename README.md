# CCA Heltec Sensor Gateway V8

**October 30, 2026 | Heltec WiFi LoRa 32 V4 OLED | ESP32-S3 | `heltec-v4`**

## V8: missing-automatic-telemetry rescue

Heltec **does not** proactively poll Cottonwood or Matheson on a fixed schedule anymore. It listens for each station's own, correctly decoded automatic stage and temperature packets. If an expected stream goes missing for **60 minutes**, it issues an encrypted sensor DM just after the deadline, as soon as the single LoRa request queue is free. Normal automatic packets reset that particular sensor's clock; battery-only and invalid stage packets do not. DM rescue replies are stored as `heltec_dm`, but do **not** reset the automatic telemetry clock. If auto stays missing, the next rescue cycle cannot begin for another 60 minutes. Maximum **three attempts**, 30 seconds between attempts, one node at a time, shared 3-second packet spacing. No infinite retries or unconditional reboots.

- **Pack Creek** `!742ecff5`: missing ultrasonic stage → `WATER READ`; missing HOBO MX2001 stage/temperature → `READ`, separately.
- **South Cottonwood** `!65165646`: missing MB7388 stage → `WATER READ`. **No HOBO sensor; no HOBO DM.**
- **Mill Creek** `!6459c5bf` and **Matheson Groundwater** `!77788479`: missing MX2001 stage/temperature → one `READ` covering both.
- **Cottonwood Relay, Hidden Valley, Fishlake Hightop, Seed Relay 1, Thousand Lake Mountain and Mini Me**: missing automatic environmental temperature → `READ` for a compatible HOBO MX2201/MX2203 temperature reader (or MX2001 where present).
- **Cottonwood Relay 2 and Spare RAK**: battery-only, no stage or temperature fallback. **Moab (Heltec's local BLE sensor)** isn't a remote DM destination.

Pack Creek SEN0313 DM stage is tagged as sensor type 3 (not MB7388 type 5). Sensor errors from other stations are never falsely recorded as South Cottonwood maintenance events.

The remote node **must implement** its matching `READ` or `WATER READ` command for rescue to work; ordinary Meshtastic environment telemetry by itself doesn't guarantee that. V8 parses MX2001, MX2201 and MX2203 replies; no synthetic sensor values are generated. Automatic telemetry and battery packets still upload normally. Owner-authorized manual `POLL ...` commands remain available, but V7 recurring poll schedules are cleared once on V8 migration to prevent unnecessary DMs. V7 unconditional Cottonwood/Matheson polling is retired, even if an old interval override is saved.

The read-only `http://192.168.1.147/json/gateway` diagnostic reports `firmware_fix: v8-missing-auto-fallback-2026-10-30` and `auto_fallback.watched` per-stream timestamps/counters. The V8 source/build branch is [Heltec-Gateway-v8](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v8). Builds are published into `downloads/Heltec-Gateway-v8/` **only after** tests and the ESP32-S3 build pass.

### VS Code PowerShell Wi-Fi OTA (after a verified V8 build)

```powershell
$ip = "192.168.1.147"
$fw = "C:\\HeltecUpdate\\Heltec-Gateway-v8-10-30-26.bin"
Invoke-WebRequest "https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v8/downloads/Heltec-Gateway-v8/Heltec-Gateway-v8-10-30-26.bin" -OutFile $fw
py -m meshtastic --host $ip --ota-update $fw
Invoke-RestMethod "http://$ip/json/gateway" | ConvertTo-Json -Depth 6
```

Only flash the non-factory application BIN. Preserve node identity, NVS, Wi-Fi, LoRa channels, encryption keys, OTA partition and existing recovery watchdog.

---

## Archived Heltec V7 documentation (rollback reference)

# CCA Heltec Sensor Gateway V7

**October 22, 2026 | Heltec WiFi LoRa 32 V4 OLED | ESP32-S3 | `heltec-v4`**

## Verified V7 release downloads

- [**Download dated V7 application BIN (Wi-Fi OTA)**](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v7/downloads/Heltec-Gateway-v7/Heltec-Gateway-v7-10-26-26.bin)
- [**Download dated V7 firmware ZIP**](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v7/downloads/Heltec-Gateway-v7/Heltec-Gateway-v7-10-26-26.zip)
- [**SHA-256 hashes, exact source commit and build log**](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v7/downloads/Heltec-Gateway-v7/BUILD.txt)
- [**V7 build and full test run (passed)**](https://github.com/canyoncountryadventure/firmware/actions/runs/39797239447)
- [**V7 recovery details and limitations**](https://github.com/canyoncountryadventure/firmware/blob/Heltec-Gateway-v7/downloads/Heltec-Gateway-v7/README.md)
- [**V6 rollback branch (unchanged)**](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v6)

**Source proven by CI:** `b929e70c2404b9c47205f7aa3a8d5fd10e7c16fe`. Binary SHA-256: `c638796a00b28c5ad39e887383ca69f72cf40a217246d7ccbd36d29c5d6db697`.

## What V7 changes

V7 preserves V6 automatic telemetry, MX2001/ultrasonic decoding, encrypted Cottonwood `WATER READ` and `READ` requests/replies, the independent DM retries, the verified ESP32 floating-point parser link fix, 90-second main-loop task watchdog, and the 12-hour preventive reboot.

The October 22 V7 update adds ordered 3-attempt/30-second CC01 requests (WATER READ first, then READ), stage-adaptive hourly or 15-minute polling above 0.75 ft, pause/resume controls in the protected Moab Watersheds dashboard, and authorized remote DM controls. Explicit CC01 HOBO unavailable and ultrasonic TIMEOUT replies now upload distinct diagnostics without fabricating sensor observations. A HOBO unavailable reply terminates that cycle immediately. The dated October 22 build passed the compatibility tests and ESP32-S3 build; the October 20 image remains available as a rollback.

The October 26 V7 update authorizes Mill Creek at Power Dam (`!6459c5bf`, decimal `1683604927`) for automatic MX2001 and device/battery telemetry. It does not add scheduled DMs to Mill Creek; the existing CC01 polling behavior is unchanged.

The October 30 V7 update polls Matheson Groundwater (`!77788479`, decimal `2004386937`) with `READ` once per hour immediately after the Cottonwood cycle completes. Cottonwood remains WATER READ then READ, with up to three attempts at 30-second gaps; Matheson then has up to three `READ` attempts at the same gap. The authorized control radio can request a one-time poll of any node with `POLL <station name|node ID>`, or add one of four persisted schedules with `POLL <station name|node ID> <minutes> [READ|WATER|BOTH]`. For example: `POLL MILL CREEK`, `POLL MILL CREEK 60 READ`, `POLL PACK CREEK 60 BOTH`, or `POLL !6459C5BF 30 READ`. `POLL <name|node> OFF` removes a saved schedule; `POLL STATUS` reports it. `POLL COTTONWOOD <minutes>`, `POLL MATHESON <minutes>`, and `POLL COTTONWOOD AUTO` retain their station-specific behavior. Accepted intervals are 5, 10, 15, 20, 30, 60, 120, 180, 360, 720 and 1440 minutes. Custom DMs run only after the Cottonwood and Matheson cycles, one target at a time, and only recognized `WATER READ` and `MX2001 READ` replies are uploaded.

The new progress supervisor runs inside the ESP32 main loop and tracks **real radio RX and hardware TX_DONE interrupts**, gateway worker progress, HTTPS POST attempts, Wi-Fi status and successful cloud uploads. It schedules recovery for a stopped worker (2 minutes), no RX *and* no completed TX despite DM requests (60 minutes), no RX with successful TX (guarded radio reinitialization at 90 minutes, reboot after 3 hours), stuck HTTP upload queue (10 minutes without POST attempts), prolonged negative HTTPS transport failures (60 minutes), and prolonged Wi-Fi loss (60 minutes). It caps consecutive **health-triggered** reboots at two until the gateway demonstrates successful RX and cloud upload during one hour of uptime. Existing scheduled Meshtastic reboots remain unchanged.

**No unconditional 3-hour reboot.** Missing radio traffic can also mean all field nodes are unreachable, not necessarily that Heltec failed. The software health supervisor cannot replace an *external* hardware watchdog for complete SoC/power lockups. Restarts can discard unsent in-RAM telemetry.

V7 moves scheduled Cottonwood RF DMs ahead of the Wi-Fi-connected check. Thus remote measurement requests still occur while Wi-Fi is unavailable, with replies queued for forwarding. The gateway diagnostic endpoint `/json/gateway` now reports supervisor RX/worker activity ages, recovery reason and reboot count. The build and V5/V6 compatibility tests all passed; field validation after flashing is still required.

### Updating the live Heltec

Use the **non-factory** `Heltec-Gateway-v7-10-26-26.bin` for the existing Meshtastic Wi-Fi Unified OTA workflow. Preserve NVS, encryption keys, channel settings, node identity and OTA partitions. The ZIP may contain factory/full-flash images which are **not** the normal Wi-Fi OTA target.

After the upgrade, check `http://192.168.1.147/json/gateway` (or the device's current LAN IP). Expected marker: `v7-matheson-poll-2026-10-30`. Verify the `cc01`, `matheson`, and `configured_polls` counters, existing automatic readings, and DM-triggered readings reach Neon.

---

## Archived V6 release notes and rollback documentation

## October 19, 2026: Independent WATER READ and READ retries

Both commands now have **independent pending flags and 120-second retry timers**, with no retry-count limit. After initial `WATER READ`, `READ` begins when a complete water response arrives or the initial water request times out; missed water responses **keep retrying even after READ starts**. READ also keeps retrying until a complete MX2001 response is queued. Neither command can cancel the other. Invalid/unavailable replies and full upload queues never count as success. Outbound CC01 DMs are spaced at least three seconds apart. The 30-minute cycle re-arms successful types without resetting in-flight retries; automatic telemetry remains enabled. `/json/gateway` exposes `cc01.water_pending`, `cc01.hobo_pending`, `cc01.water_retries`, and `cc01.hobo_retries`; the firmware marker is `v6-cc01-dual-retry-2026-10-19`. **The dual-retry BIN is now built and published.**

## October 19, 2026: ESP32 floating-point parser repair

**Embedded parser root cause:** the previous ESP32 image linked `sscanf` to its integer-only scanner without the optional `_scanf_float` implementation. Both CC01 reply parsers use `%f`, so native tests passed while the radio rejected the same valid text. The live gateway reported three replies, zero parsed and three format rejections; its other uploads received HTTP 201.

V6 now retains the floating-point scanner with `-Wl,-u,_scanf_float` for `heltec-v4`. CI checks that the compiled ELF defines that symbol before publishing. The read-only `/json/gateway` response runs the real parser on both screenshot messages and reports `parser_self_test.water` and `parser_self_test.hobo`. These self-tests never queue or upload synthetic observations.

**Earlier parser-repair build:** source `4ed0ed8a7ebbb82eb4d93d3d5809a296edbfbaa8`, firmware `2.7.26.4ed0ed8`. That build verified the `_scanf_float` linker repair but predates independent WATER READ retries. The new dual-retry build is listed below. On-radio DM and cloud verification is still required after flashing.

The previous receive handler silently ignored valid CC01 sensor replies unless the poller was waiting for that exact response type. This could discard manual responses, late replies after a two-minute timeout, or replies arriving out of order. V6 now parses complete, authorized CC01 `WATER READ` and `MX2001` replies in every polling state and queues them through the existing HTTPS uploader. Only a matching response advances the scheduled poll; unrelated replies do not cancel or restart it. Automatic private packets and the 30-minute polling schedule are unchanged.

**Wi-Fi-only diagnosis:** open `http://192.168.1.147/json/gateway` (use the gateway's current IP) or run `Invoke-RestMethod http://192.168.1.147/json/gateway | ConvertTo-Json -Depth 5`. This read-only status endpoint reports the V6 fix identifier, uptime, Wi-Fi, whether an ingest credential exists (never its value), queue usage, total accepted uploads, and CC01 requests/replies/parsed/queued/rejected counts, last reply packet ID and last HTTP result. `cc01.uploaded > 0` with HTTP 2xx confirms the cloud accepted a DM reading; confirm its presence in Neon separately. Counters reset on reboot. No USB or serial monitoring is required.

Regression coverage replays the actual production poller with the October 19 screenshot replies in every state, including late replies, full queues, invalid readings and the existing startup/timeout schedule. The packet capture proved that a valid encrypted CC01 reply reached Heltec; it did not expose the poll state or HTTP result, so the timer gate is a confirmed discard path rather than a proven explanation for every missing historical packet.


**Updated October 19, 2026 | Heltec WiFi LoRa 32 hardware V4 OLED | ESP32-S3 | PlatformIO `heltec-v4`**

Firmware V6 includes scheduled CC01 polling and the encrypted-DM receive fix. The initial V6 build sent scheduled requests but rejected PKI-encrypted CC01 responses because Meshtastic decodes those DMs on local channel 0, not LongFast channel 1. V6 accepts an addressed reply only from CC01 on authenticated PKI channel 0 or legacy LongFast channel 1, adds parser-failure diagnostics, and no longer calls an unconfirmed send attempt 'queued'. Existing automatic DS/MX packets, all other station handlers, calibration, Wi-Fi, BLE, watchdog, OTA, and the RAK firmware remain unchanged.

## Verified V6 downloads and build provenance

- [**Download Heltec V6 application BIN, October 19, 2026**](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v6/downloads/Heltec-Gateway-v6/Heltec-Gateway-v6-10-19-26.bin)
- [**Download Heltec V6 OTA/full-flash ZIP, October 19, 2026**](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v6/downloads/Heltec-Gateway-v6/Heltec-Gateway-v6-10-19-26.zip)
- [**Source commit and SHA-256 checksums**](https://raw.githubusercontent.com/canyoncountryadventure/firmware/Heltec-Gateway-v6/downloads/Heltec-Gateway-v6/BUILD.txt)
- [**Build and regression-test logs**](https://github.com/canyoncountryadventure/firmware/actions/runs/39288107998)
- [Rollback: Heltec V5 (automatic telemetry only)](https://github.com/canyoncountryadventure/firmware/tree/Heltec-Gateway-v5)
- [RAK field firmware V6 (unchanged, separate UF2)](https://github.com/canyoncountryadventure/firmware/tree/RAK-Water-Distance-HOBO-v6)

| Published file | SHA-256 |
| --- | --- |
| `Heltec-Gateway-v6-10-19-26.bin` | `2fa4cc606f29af361a4e8121523d9e9997bbb33bc927571eb1d343a7a2fc0e6e` |
| `Heltec-Gateway-v6-10-19-26.zip` | `29caf103e57b8ffcad52fdc367ede8161bfc0a90ac40dc867a50fa2f4f90fa25` |

Current dual-retry source commit: [`ae7ae62d33220ec78c6a53a2a25f3b42066abf21`](https://github.com/canyoncountryadventure/firmware/commit/ae7ae62d33220ec78c6a53a2a25f3b42066abf21). Published artifact commit: `2264c0ca6ad110d1abe9e2a4e59ba8495f227451`. V6 historical documentation was written before the new V7 health-supervisor branch; retain V6 as the rollback image.

**This is Heltec hardware V4 firmware, not a RAK UF2.** The standalone BIN is the application image for routine Wi-Fi Unified OTA. Preserve NVS/configuration and the existing OTA loader. The ZIP also contains a factory image, which is not the routine Wi-Fi update target.

### Wi-Fi flash from PowerShell

Close any TCP monitor first. Download and verify the current application, then use Meshtastic's Wi-Fi updater:

```powershell
$ErrorActionPreference = "Stop"
$firmwarePath = Join-Path $PWD "Heltec-Gateway-v6-10-19-26.bin"
Invoke-WebRequest -UseBasicParsing "https://raw.githubusercontent.com/canyoncountryadventure/firmware/2264c0ca6ad110d1abe9e2a4e59ba8495f227451/downloads/Heltec-Gateway-v6/Heltec-Gateway-v6-10-19-26.bin" -OutFile $firmwarePath
if ((Get-FileHash $firmwarePath -Algorithm SHA256).Hash -ne "2fa4cc606f29af361a4e8121523d9e9997bbb33bc927571eb1d343a7a2fc0e6e") { throw "Firmware checksum mismatch" }
$ErrorActionPreference = "Continue"
python -m meshtastic --host 192.168.1.147 --ota-update $firmwarePath
if ($LASTEXITCODE -ne 0) { throw "Wi-Fi updater failed; inspect its output" }
```

After Heltec reconnects, verify `firmware_fix` is `v6-cc01-dual-retry-2026-10-19` and both `parser_self_test` values are `true`. Then inspect `http://192.168.1.147/json/gateway`. Use the actual current IP if it changed. A manual `READ` sent from Heltec to `!65165646` can now be ingested immediately. Verify `cc01.uploaded` increases and the new reading is present in Neon with `source: "heltec_dm"`. Scheduled first-poll delay and 30-minute interval are unchanged.

## Encrypted-DM receive repair included in V6

On Meshtastic 2.7.26, a PKI-encrypted direct message is decrypted for the destination with **local channel index 0**, regardless of the LongFast index chosen when composing the request. The initial V6 `wantPacket()` accepted CC01 `TEXT_MESSAGE_APP` replies only if `channel == 1`. Valid encrypted replies were dropped before `handleCc01Reply()`. V6 fixes the receive filter while preserving sender/destination authorization and the existing LongFast fallback.

V6 also reports outbound requests as **attempted, delivery unconfirmed** rather than claiming the RF network queued/delivered them, and logs when a CC01 reply arrives but does not match the strict `WATER READ` or `MX2001` parser. The 2-minute initial delay, 30-minute polling, reply timeouts, and unconditional automatic packet handling are unchanged.

**On-device validation still required:** reboot V6 on Heltec with Wi-Fi online, observe `CCA CC01 DM poll: sent request WATER READ`, then verify at least one decoded CC01 DM reply is uploaded with `metrics.source = "heltec_dm"` in `moab_watersheds.creek_observations`. This distinguishes sending, receiving/parsing and HTTPS storage. Compilation alone does not prove LoRa delivery.

## Scheduled DM behavior

Every **30 minutes**, Heltec sends two separate Meshtastic direct messages to **CC01 / Cottonwood Creek `!65165646`** over the gateway's local **LongFast channel 1**:

1. **`WATER READ`**: requests a fresh MB7388 ultrasonic reading. Only a complete, calibrated `WATER READ` reply is converted to a normal `water_distance` observation with raw distance (mm) and stage (ft/mm).
2. **`READ`**: requests a fresh HOBO MX2001 BLE read on CC01. A complete `MX2001` reply becomes an `mx2001` observation with logger MAC, water level, temperature F/C and BLE RSSI. Text has two-decimal-foot and one-decimal-F precision because that is what the **RAK V6 READ DM currently returns**. No extra precision is fabricated.

The first cycle starts around **2 minutes after Heltec boots**; following cycles start every 30 minutes. Heltec waits up to **2 minutes** for the WATER reply, then sends READ after a short spacing delay and waits up to 2 minutes for that reply. A missing WATER reply advances to READ after two minutes. Missing or invalid WATER READ and READ replies independently trigger another request approximately every two minutes until each reading enters the upload queue. A full queue never stops either retry. Both remain active concurrently after the initial water timeout, and transmissions are spaced at least three seconds apart. The next 30-minute cycle still polls water before resuming HOBO requests. Unrelated text DMs cannot produce readings. No request is considered delivered merely because Meshtastic queued it.

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
