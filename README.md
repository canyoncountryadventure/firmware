> **October 17, 2026 development branch. NOT a verified firmware release.** Branched from RAK-Water-Distance-HOBO-v5. Existing binary links below refer to prior releases and must not be represented as V5/V6 downloads. No new UF2, OTA, or Scout image has been built or validated yet. Do not flash this branch until matching dated artifacts, SHA-256 checksums, CI results, and hardware verification are published.

# RAK Water Distance + HOBO V6 (development)

RAK4631/RAK19007 monitoring firmware for **DFRobot SEN0313/A01NYUB** or **MaxBotix MB7388**, with HOBO **MX2001**, **MX2201** and **MX2203** support.

V5 retains the HOBO/BLE implementation that successfully read the MX2001 on October 15. Water calibration and live measurements now use the median of 10 valid fresh readings after one warm-up. It keeps the full eight-second sensor settling time while returning control to the HOBO/mesh scheduler, uses V3's passive 10% scan, and releases its temporary CPU wake/clock requests after a valid logger STATUS baseline. [Full version audit and rationale](docs/diagnostic-8/V5-FINAL-AUDIT.md).

## Downloads

<!-- V5_RELEASE_START -->
Verified firmware: **Meshtastic 2.7.26.83c1638**, source [`83c1638`](https://github.com/canyoncountryadventure/firmware/commit/83c163807015add224ee1878350c3fec6e0d29bf), built October 16, 2026. Both field flavors and their exact matched autonomous Scouts passed [build/package verification](https://github.com/canyoncountryadventure/firmware/actions/runs/38953447072). Selected package: nonblocking **8 s** sensor settling followed by a **10-valid-sample median** for live readings, calibration and verification (20-attempt limit), passive **10%** HOBO scan, bounded CPU wake/HFCLK requests released at the first valid STATUS baseline, and preserved saved settings. `VERSION` reports this exact build and target. [Operating choices and MX2001 evidence](docs/diagnostic-8/V5-FINAL-AUDIT.md).

| Distance sensor | Monitoring station — USB | Field application — BLE DFU | Separate drone radio | Scout pairing |
| --- | --- | --- | --- | --- |
| SEN0313 | [Field UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/field-rak-hobo-distance-10-16-26-SEN0313.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/field-rak-hobo-distance-10-16-26-SEN0313-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/drone-rak-hobo-distance-10-16-26-SEN0313.uf2) | [Manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/drone-rak-hobo-distance-10-16-26-SEN0313.uf2.txt) |
| MB7388 | [Field UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/field-rak-hobo-distance-10-16-26-MB7388.uf2) | [OTA ZIP](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/field-rak-hobo-distance-10-16-26-MB7388-OTA.zip) | [Drone UF2](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/drone-rak-hobo-distance-10-16-26-MB7388.uf2) | [Manifest](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/drone-rak-hobo-distance-10-16-26-MB7388.uf2.txt) |

[SHA-256 checksums](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/SHA256SUMS.txt) · [Build source and provenance](https://raw.githubusercontent.com/canyoncountryadventure/firmware/RAK-Water-Distance-HOBO-v5/downloads/RAK-HOBO-v5-Audit-Repair/BUILD.txt) · [Flashing/rollback](docs/diagnostic-8/FLASHING.md)
<!-- V5_RELEASE_END -->

**Field UF2 → monitoring station. Drone UF2 → separate RAK4631 carried by the drone.** The OTA ZIP is the station application for Nordic/Adafruit Legacy BLE DFU. Each drone image embeds the exact matching station application; an old drone image installs its old embedded firmware.

Download names use `field-rak-hobo-distance-MM-DD-YY-SENSOR` or `drone-rak-hobo-distance-MM-DD-YY-SENSOR`, with the build date in Mountain time. Renamed October 16 files are byte-for-byte identical to verified build `83c1638`.

Choose the sensor actually installed. Pack Creek/Swell use SEN0313; Cottonwood uses MB7388 when that sensor is installed. Builds are not tied to a station ID. A valid saved sensor selection overrides the build default.

## Why the MX2001 was failing

V3 had no switched-sensor warm-up. V4 added Pololu control and increased the blocking warm-up from one second to three, then eight. Early V5 inherited it. That blocked other jobs on Meshtastic's cooperative main controller; yielding inside the delay did not run the HOBO state machine. The BLE interrupt/event task is separate, so this does not mean every BLE interrupt stopped.

The last functional update before success replaced the blocking wait with scheduled water jobs. The October 15 log at 11:19 contains `STATUS model=MX2001 pointer=0x00001C15 interval=3600`. Earlier wake-only changes still failed. **Scheduler contention is the leading explanation; the available evidence does not prove the trigger of every prior GATT failure.** The log includes older October 14 failures and must not be read as one continuous test of the successful build.

## Final operating settings

| Function | Selected behavior |
| --- | --- |
| Sensor power | Pololu ON from IO1/P0.17; full 8 s settling; OFF between jobs |
| Sensor scheduling | Warm-up returns to scheduler; one sample job owns the rail; busy requests receive a reply |
| Live / READ / RAW / VERIFY / CAL | Median of 10 valid fresh readings after one warm-up; up to 20 attempts; incomplete bursts fail |
| BLE acquisition | Passive 200 ms period / 20 ms receive window (10%); no periodic restart of healthy scanning |
| Connection / setup | Existing 30 s connection limit; V3 callback discovery, three bounded attempts per step |
| Handshake power | CPU wake + HFCLK request from module creation, maximum 120 s; release at first valid STATUS baseline |
| Failed acquisition | 5 min wake-request cooldown; normal idle sleep permitted while mesh/passive scan continue |
| Healthy HOBO link | Normal idle sleep permitted; STATUS checks every 30 s; automatic TX only on a new logger record |
| Water interval | Preserve saved interval; fresh-install default one hour |
| Recovery | Core/radio watchdog and existing 12-hour maintenance reboot; ordinary HOBO errors retry without forced watchdog resets |
| Feedback | Three red flashes on failed setup; three simultaneous RGB flashes on verified handshake |

Ten-sample medians average the two middle distances and round half millimeters upward to the packet’s whole-millimeter resolution. `WATER READ` and `RAW` report the sample count and spread. A burst uses one power-on window and yields between readings; no long sampling loop blocks the HOBO scheduler. Existing calibration, identity, channels, keys, logger lock, and reporting interval are retained by application-only updates.

Single driver reads still have bounded waits: up to 450 ms for DFRobot UART and 3 s for MB7388. The eight-second power warm-up is nonblocking. The 5 min cooldown does not shut down the mesh or scanner.

## Fixes retained in V5

- Scanner resume/restart after rejected initiation or cancellation, with lifecycle guards against overlapping scan/connect operations.
- Phone/USB Bluetooth shutdown disconnects peripheral phone links without dropping the independent HOBO central link.
- Failed water reads preserve sensor selection and calibration; no automatic MB migration that clears calibration.
- Current model identification is separate from last verified logger identity. LittleFS replaces identity and MAC-lock records rather than leaving an older record at the start of an appended file.
- Existing MX2001 stage/temperature, MX2201/MX2203 temperature, logger-pointer synchronization, packet formats, calibration storage, node identity, keys, channels and remote DFU remain.
- `VERSION` reports the actual firmware build and target environment. The final label is V5, without the superseded HFCLK diagnostic suffix.

The full V1–V5 history, including rejected 50% scanning, bench toggles and watchdog escalation, is in the [final audit](docs/diagnostic-8/V5-FINAL-AUDIT.md). Older branches are unchanged.

## Install on the field station

1. Record `VERSION`, `LOGGER`, `WATER STATUS` and `WATER CAL STATUS` before the update.
2. For USB: double-reset into the existing RAK UF2 bootloader, then copy the matching **Field UF2**. For BLE DFU: use the matching **OTA ZIP**.
3. After startup, check those same commands and compare stored calibration, sensor selection and logger lock. VERSION now includes the commit-based build and target flavor.

Station packages contain application addresses only. A firmware-only upgrade does not request a factory reset, filesystem erase or calibration reset. Existing explicit sensor-change/reset commands can change calibration; they are not upgrade steps.

## Install on the drone flasher

1. Put the matching **Drone UF2** on the **separate drone RAK4631**. Its manifest must match this release's station source commit and OTA checksum.
2. Power the prepared Scout and send `DFU` as a mesh DM to the intended field station. Enter DFU on only the intended target during that operation.
3. Let the Scout transfer its embedded application, then verify the station's post-update callback, `VERSION` and saved calibration.

The Scout uses the existing Legacy DFU implementation pinned at `37306158a64266fd05dfcd3bf01c5b47d0dc35fa`. For the separate Phase-2 serial Scout bridge, use the matching OTA ZIP with `tools/rak_dfu_serial_upload.py` from that implementation. [Detailed flashing and rollback](docs/diagnostic-8/FLASHING.md).

## Direct-message commands

Bare `READ` reads HOBO. Prefix water commands with `WATER`.

| Command | Result |
| --- | --- |
| `VERSION` | V5 label, actual firmware build and target environment |
| `LOGGER` | Current model, BLE state, MAC, lock, interval and last error |
| `READ` | Current HOBO reading |
| `LOCK` / `LOCK AA:BB:CC:DD:EE:FF` | Persist connected verified logger / explicit target MAC |
| `UNLOCK` | Resume unrestricted logger selection |
| `WATER STATUS` / `WATER READ` | Sensor configuration and refreshed water reading |
| `WATER CAL STATUS` | Stored calibration and stage reference |
| `WATER VERIFY` | Five fresh readings and spread after one warm-up |
| `WATER SENSOR SEN0313` / `WATER SENSOR MB7388` | Explicit sensor selection |
| `WATER TELEMETRY NOW` | Sample and queue water telemetry |
| `STATS` / `WATCHDOG` | Reset, wake/clock and watchdog diagnostics |
| `REBOOT` / `RECOVER` | Explicit watchdog-origin hardware reset; allow approximately 90 s after arming |
| `SOFT REBOOT` | Conventional software reset after three seconds |
| `DFU` | Enter existing bootloader for a prepared update |
| `HELP` / `WATER HELP` | Complete command syntax, including intervals and calibration |

`SCAN` / `RECONNECT` report automatic lifecycle ownership. `Not identified` means the model has not been verified; a candidate MAC or RSSI is insufficient. `gatt=FFFF` with `rsp=0` is a missing-response sentinel. `hobo_wake=0` means the HOBO sleep veto ended; `hobo_hf` reports crystal activity, which can also be required by the BLE stack.

## Verification

Both sensor builds, application-only DFU packages and exact matched Scout payloads pass the [build workflow](.github/workflows/build_rak_water_v5.yml) before publication. It checks chip/address/source identity, DFU CRC and field-UF2 equivalence, embedded Scout application/DAT equivalence, LZ4 decoding, lifecycle fault injection, protocol/storage preservation, reset replay protection and executable water-job scheduling regressions.

The October 15 MX2001 response is a reported hardware success. Repeated battery-only startup/reconnection, correct stage values, automatic hourly records, long-duration power use, calibration retention through physical update and airborne transfer have not all been verified. Build/package checks do not establish those results. [Validation record](docs/diagnostic-8/VALIDATION.md) · [Version provenance](docs/diagnostic-8/water-version-provenance.json).
