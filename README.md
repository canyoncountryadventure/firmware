# RAK Water Distance + HOBO V5

RAK4631 firmware for **SEN0313/A01NYUB** or **MaxBotix MB7388** distance sensing, plus HOBO **MX2001**, **MX2201**, and **MX2203** loggers. Sensor builds are not tied to a station ID. Pack Creek/Swell use SEN0313; Cottonwood uses MB7388 when that sensor is installed.

## Downloads

<!-- V5_RELEASE_START -->
Verified firmware: **Meshtastic 2.7.26.3580e56**, source [`3580e56`](https://github.com/canyoncountryadventure/firmware/commit/3580e56c211187735eccdc27cb69c76034404df1), built October 15, 2026. Both field flavors and their exact matched autonomous Scouts passed [build/package verification](https://github.com/canyoncountryadventure/firmware/actions/runs/38672506236). This build keeps the nRF52 awake from HOBO module creation—before its first battery-mode idle sleep—through the bounded handshake, then restores normal sleep immediately after the first verified `STATUS` baseline. Remote `STATS` exposes the active state as `hobo_wake`.

| Distance sensor | Monitoring station — USB | Field application — BLE DFU | Separate drone radio | Scout pairing |
| --- | --- | --- | --- | --- |
| SEN0313 | [Field UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/RAK-HOBO-v5-Audit-Repair-SEN0313.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/RAK-HOBO-v5-Audit-Repair-SEN0313-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/Drone-RAK-HOBO-v5-Audit-Repair-SEN0313.uf2) | [Manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/Drone-RAK-HOBO-v5-Audit-Repair-SEN0313.uf2.txt) |
| MB7388 | [Field UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/RAK-HOBO-v5-Audit-Repair-MB7388.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/RAK-HOBO-v5-Audit-Repair-MB7388-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/Drone-RAK-HOBO-v5-Audit-Repair-MB7388.uf2) | [Manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/Drone-RAK-HOBO-v5-Audit-Repair-MB7388.uf2.txt) |

[SHA-256 checksums](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/SHA256SUMS.txt) · [Build source and provenance](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/BUILD.txt) · [Flashing/rollback](docs/diagnostic-8/FLASHING.md)
<!-- V5_RELEASE_END -->

**Field UF2 goes on the monitoring station. Drone Scout UF2 goes on the separate RAK4631 carried by the drone.** The OTA ZIP is the field application for Nordic/Adafruit Legacy BLE DFU. An autonomous Scout carries its target image inside it; an old Scout will install its old embedded firmware.

## October 15 update: battery-mode HOBO handshake

The first battery-mode test proved that suppressing Meshtastic's long idle sleep was not sufficient: `hobo_wake=1` remained active while service discovery still returned `rsp=0 gatt=FFFF n=0`. The follow-up build therefore also holds the nRF52840 high-frequency crystal during the same bounded handshake window, matching the important clock-side effect of an active USB data session. `STATS` reports `hobo_hf=1` when the crystal is confirmed running and `hf_rc` for the SoftDevice request/release result. Both the CPU wake and crystal request end after the first valid logger `STATUS` baseline or the existing 120-second failure budget.

V5 also fixes saved logger identity and explicit MAC-lock replacement on nRF52 LittleFS. Its write mode appends; the old implementation could leave an earlier MX2203 record first in the file even after an MX2001 had verified. The fixed-size record is now replaced, so `Last MAC` and changed locks survive reboot correctly. This reporting/storage bug occurs after logger identification and did not cause the earlier GATT discovery failure.

Field testing isolated the difference between the failing and working cases: a computer USB **serial session** kept the RAK awake and the MX2001 handshake completed, while USB power alone behaved like battery power and did not fix it. USB voltage was therefore not the useful variable.

- V5 now requests continuous CPU wake and the nRF52840 high-frequency crystal from HOBO module creation—before the first battery-mode idle-loop sleep—through scanning, connection, GATT discovery, logger identification, and the first valid `STATUS` baseline.
- Both requests are released immediately after that verified baseline, so normal nRF52 sleep resumes while the established BLE link and ordinary 30-second status checks continue. `STATS` reports `hobo_wake`, confirmed crystal state as `hobo_hf`, and the SoftDevice clock result as `hf_rc`.
- A missing or non-responsive logger gets a maximum **120-second awake window**, followed by a **5-minute sleep/cooldown** before another candidate can start a new window. A healthy link that later disconnects receives a fresh bounded reconnect window.
- BLE connection/discovery timeouts, passive 10% scan duty, logger recording interval, mesh telemetry cadence, calibration, and saved logger identity/lock were not changed.
- The Safe V3/V4 branches were checked before this change. They use the same platform rule—sleep is disabled only by an active USB serial session—and do not contain a separate HOBO wake lock to copy. V5 preserves their bounded BLE discovery flow and adds this narrowly scoped handshake owner.
- CI exercises acquisition, deadline expiry, cooldown, success release, and reconnect re-arming. Physical battery-mode RAK/MX2001 validation is still required before calling the field issue solved.

## October 14 update: remote watchdog reset

- Text DM `REBOOT` or `RECOVER` now requests a **watchdog-origin hardware reset**, rather than the ordinary SoftDevice shutdown and CPU-only reset. The command reply transmits first; the watchdog is starved after three seconds and resets the nRF52840 within its approximately 90-second window.
- A validated replay marker in retained `.noinit` RAM rejects the same sender/packet-ID reboot command after restart. This specifically addresses reboot packet retransmission across boots.
- `SOFT REBOOT` retains the prior three-second conventional reset for comparison. Ordinary system and DFU update paths are unchanged.
- The reset reason is reported by `STATUS` or `STATS`: `0x2` indicates watchdog reset, `0x1` reset pin, and `0x4` software reset. This is an internal hardware reset, **not** a power cycle, and field equivalence to the reset button has not yet been proven.
- Calibration, saved sensor selection, MX2001 tracking, mesh identity, and wire protocol were not changed. This release requires field testing before a reliability claim.

## What changed on October 14

- Restored V3's bounded service, characteristic and notification discovery in the deferred connection callback. The pinned Bluefruit library's separate BLE event task can complete those requests while the callback waits.
- Restored V3's passive **10% scan duty** and removed periodic restarts of healthy scanning.
- Corrected a second-audit scanner stall: a failed stop plus rejected connection request, or cancellation after a failed stop, can leave Bluefruit's cached running flag true. Those failure paths now explicitly request resume/restart after the existing five-second backoff. A pending or live connection still blocks scanner restart.
- Ordinary HOBO BLE failures disconnect/cancel and retry without tripping the hardware watchdog. The existing core/radio watchdog and 12-hour scheduled reboot remain.
- Phone/USB-serial Bluetooth shutdown disconnects peripheral phone links, preserving the independent HOBO central link. Actual handles are checked rather than assuming contiguous numbering.
- A failed distance read no longer automatically switches sensor type and clears saved calibration. Explicit sensor-selection/calibration commands remain.

The second-audit regression reproduced the scanner stall before the correction and passed afterward. **This establishes that failure path, not the physical cause of either station's October 14 error.** Swell's missing service response and Cottonwood's connection timeout are separate failures; V4's onset remains unproven. Full evidence and confidence levels are in the [audit](docs/diagnostic-8/README.md) and [second review](docs/diagnostic-8/SECOND-AUDIT.md).

## What remains intact

MX2001 water level/temperature; MX2201 and MX2203 temperature; automatic logger discovery; persistent manual MAC locks; truthful current model and last verified identity in `LOGGER`; logger interval/write-pointer synchronization; automatic telemetry on new records; SEN0313, MaxBotix and existing sensor power control; water distance, stage and discharge; existing calibration storage; Meshtastic identity/channels/encryption; and the existing remote DFU hook.

No factory reset, filesystem erase, calibration reset or timeout increase was introduced. The text-DM watchdog reset is explicitly requested; ordinary BLE retry does not automatically request it. A stored valid sensor configuration wins over the build's default.

## Update the field station

For USB, enter the existing RAK4631 UF2 bootloader and copy the matching **field UF2**. These images contain application addresses only. After startup, compare `VERSION`, `LOGGER`, `WATER STATUS` and `WATER CAL STATUS` with the pre-update values.

For autonomous drone update, first install the matching **Drone Scout UF2 on the separate drone radio**. Check the manifest's target source commit and OTA checksum against this release. Power the Scout, send `DFU` as a mesh DM to the intended field station, and let the Scout transfer its embedded application. Enter DFU on only the intended target during that operation. Confirm the post-update mesh callback and `VERSION`.

For the existing Phase-2 serial Scout bridge, use the matching OTA ZIP with `tools/rak_dfu_serial_upload.py` from the pinned drone implementation. A ZIP alone does not update an autonomous Scout's embedded image.

See [flashing and rollback instructions](docs/diagnostic-8/FLASHING.md). Preserve the prior app-only field image and its matched Scout for rollback. Do not change sensor selection or reset calibration as part of a firmware-only upgrade. Existing explicit sensor-change/reset commands can change calibration.

## Useful DMs

Send commands directly to the station. Bare `READ` reads HOBO; water commands use the `WATER` prefix.

| Command | Result |
| --- | --- |
| `VERSION` | Firmware build/version |
| `LOGGER` | Model, BLE state, MAC, lock, interval and last error |
| `READ` | Current HOBO reading |
| `LOCK` | Save the connected, identified logger as the target |
| `LOCK AA:BB:CC:DD:EE:FF` | Save an explicit target MAC |
| `UNLOCK` | Resume unrestricted automatic logger selection |
| `WATER STATUS` | Sensor and water configuration |
| `WATER READ` | Current distance/stage reading |
| `WATER CAL STATUS` | Stored calibration, including the stage reference |
| `WATER SENSOR SEN0313` | Explicit SEN0313 selection |
| `WATER SENSOR MB7388` | Explicit MaxBotix selection |
| `WATER TELEMETRY NOW` | Request water telemetry |
| `HELP`, `WATER HELP` | Complete on-device commands |
| `BLE`, `DIAG`, `STATS`, `WATCHDOG` | Diagnostics |
| `REBOOT` or `RECOVER` | Schedule watchdog-origin hardware reset; can take up to approximately 90 seconds after arming |
| `SOFT REBOOT` | Retain conventional fast CPU-only reset for comparison |
| `DFU` | Enter the existing bootloader for the prepared update |

`SCAN` and `RECONNECT` report automatic state-machine ownership; they do not force a manual restart. “Not identified” means the firmware has not verified the current candidate's model. A MAC/RSSI alone is not logger identification; FFFF in a missing-response discovery error is a sentinel, not a decoded GATT error.

## Validation

[Validation record](docs/diagnostic-8/VALIDATION.md) · [Seven-branch provenance](docs/diagnostic-8/provenance.json) · [Build workflow](.github/workflows/build_rak_water_v5.yml)

The first audit passed 162 Python host tests, lifecycle fault injection and 20 protocol/storage preservation checks. Both field builds and their matched autonomous Scouts passed GitHub builds and package verification. The broad native suite could not build in the audit host because Linux dependencies were missing; it is not counted as passing.

The second audit adds regression coverage for stale scanner flags after connection rejection and cancellation. New downloads are published only after both field flavors, their application-only DFU packages, and exact matched Scout payloads pass validation.

**Physical hardware testing remains:** logger identity at both sites, all three models, RF reconnection, battery/sensor-power coexistence, long-duration operation, calibration/configuration retention and actual drone transfer. Software tests and valid packages do not establish those results.
